#!/usr/bin/env python3
"""从 TTF 生成 LVGL fmt_txt 子集字体(4bpp)。

本仓库没有 node/lv_font_conv,所以用 fontTools 读取字形轮廓,自写扫描线抗锯齿
栅格器输出 LVGL 9 的 `lv_font_fmt_txt` 格式。字符清单不是手工维护的:脚本扫描
main/*.c 的字符串字面量(跳过注释),因此只要界面文案改了,重新生成一次即可。

用法(从仓库根目录运行):

    python tools/gen_cjk_font.py \
        --font assets/fonts/WenQuanDengKuanWeiMiHei-1.ttf \
        --size 16 --name app_font_16 --output assets/fonts/app_font_16.c

生成的 C 文件结构与 lv_font_conv 输出一致:连续 4bpp 半字节流(每字节高半字节为
左侧像素、行间不补位),cmap 用 ASCII 连续段 + 非 ASCII 稀疏段,并带 stride=0。
"""

from __future__ import annotations

import argparse
import math
import pathlib
import re
import sys
from typing import Dict, List, Sequence, Set, Tuple

from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.ttLib import TTFont

ASCII_START = 0x20
ASCII_END = 0x7E
# 每像素的超采样边数(SS x SS 个采样点)。8 时每像素 64 个采样点,细笔画的小字号
# 也能得到更准确的覆盖度,避免整体发灰。
SUPERSAMPLE = 8
# 每条二次曲线的细分数,16px 下足够平滑。
QUAD_STEPS = 8
# 行高留白(像素)。字体自带的 hhea 行高通常远大于墨迹(本字体 1.31em),会把文字在
# 各自的行框里往下推;这里改为按子集墨迹计算行高,只留这一点点空隙,使绘制坐标
# 就等于墨迹顶部。
LEADING_PX = 2


# ---------------------------------------------------------------------------
# 字符清单:直接来自应用源码里的字符串字面量
# ---------------------------------------------------------------------------
def collect_codepoints(paths: Sequence[pathlib.Path]) -> Set[int]:
    """收集 C 源码字符串字面量里的码点,跳过注释与转义序列。"""
    codepoints: Set[int] = set(range(ASCII_START, ASCII_END + 1))

    for path in paths:
        src = path.read_text(encoding="utf-8")
        i = 0
        n = len(src)
        while i < n:
            c = src[i]
            if c == "/" and src[i + 1 : i + 2] == "/":
                while i < n and src[i] != "\n":
                    i += 1
            elif c == "/" and src[i + 1 : i + 2] == "*":
                i += 2
                while i + 1 < n and not (src[i] == "*" and src[i + 1] == "/"):
                    i += 1
                i += 2
            elif c == '"':
                i += 1
                while i < n and src[i] != '"':
                    if src[i] == "\\":
                        i += 2
                        continue
                    # 控制字符(如续行留下的换行)不参与字形清单。
                    if ord(src[i]) >= 0x20 and ord(src[i]) != 0x7F:
                        codepoints.add(ord(src[i]))
                    i += 1
                i += 1
            else:
                i += 1
    return codepoints


# ---------------------------------------------------------------------------
# 轮廓解析:fontTools -> 位图坐标系下的折线轮廓
# ---------------------------------------------------------------------------
def _quadratic_points(current, points):
    """展开 qCurveTo:连续的控制点之间插入隐含的曲线上点。"""
    pts = list(points)
    out = []
    if len(pts) == 1:
        return None
    for i in range(len(pts) - 1):
        control = pts[i]
        if i == len(pts) - 2:
            end = pts[-1]
        else:
            following = pts[i + 1]
            end = ((control[0] + following[0]) / 2.0, (control[1] + following[1]) / 2.0)
        out.append((control, end))
    return out


def glyph_contours(glyph_set, name) -> List[List[Tuple[float, float]]]:
    """返回字形轮廓(字形单位坐标的折线)。

    用 DecomposingRecordingPen 展开复合字形:汉字常由部件拼成,如果不展开,
    这些字会得到空轮廓而在屏上消失。
    """
    pen = DecomposingRecordingPen(glyph_set)
    glyph_set[name].draw(pen)

    contours: List[List[Tuple[float, float]]] = []
    current: List[Tuple[float, float]] = []
    cur_pt = None

    def quad(p0, c, p1):
        for step in range(1, QUAD_STEPS + 1):
            t = step / QUAD_STEPS
            mt = 1.0 - t
            x = mt * mt * p0[0] + 2 * mt * t * c[0] + t * t * p1[0]
            y = mt * mt * p0[1] + 2 * mt * t * c[1] + t * t * p1[1]
            current.append((x, y))

    for op, args in pen.value:
        if op == "moveTo":
            if len(current) > 1:
                contours.append(current)
            current = [args[0]]
            cur_pt = args[0]
        elif op == "lineTo":
            current.append(args[0])
            cur_pt = args[0]
        elif op == "qCurveTo":
            pairs = _quadratic_points(cur_pt, args)
            if pairs is None:
                current.append(args[0])
            else:
                for control, end in pairs:
                    quad(cur_pt, control, end)
                    cur_pt = end
        elif op == "curveTo":
            raise SystemExit(f"{name}: 存在三次曲线,本脚本只处理 TrueType 二次轮廓")
        elif op == "closePath":
            if len(current) > 1:
                contours.append(current)
            current = []
    if len(current) > 1:
        contours.append(current)
    return contours


# ---------------------------------------------------------------------------
# 栅格化:扫描线 + 超采样
# ---------------------------------------------------------------------------
def rasterize(contours, scale, box_w, box_h, origin_x, top_y):
    """把字形轮廓栅格化成 box_w x box_h 的 0..15 覆盖度。"""
    edges = []
    for contour in contours:
        pts = [(x * scale, y * scale) for x, y in contour]
        for i in range(len(pts)):
            x0, y0 = pts[i]
            x1, y1 = pts[(i + 1) % len(pts)]
            if y0 == y1:
                continue
            # 位图坐标:向右为 +x,向下为 +y(以字形包围盒左上角为原点)
            bx0 = x0 - origin_x
            by0 = top_y - y0
            bx1 = x1 - origin_x
            by1 = top_y - y1
            edges.append((bx0, by0, bx1, by1))

    nibbles = [0] * (box_w * box_h)
    if box_w == 0 or box_h == 0 or not edges:
        return nibbles

    total = SUPERSAMPLE * SUPERSAMPLE
    for row in range(box_h):
        counts = [0] * box_w
        for sub_row in range(SUPERSAMPLE):
            y = (row * SUPERSAMPLE + sub_row + 0.5) / SUPERSAMPLE
            crossings = []
            for bx0, by0, bx1, by1 in edges:
                if (by0 <= y < by1) or (by1 <= y < by0):
                    t = (y - by0) / (by1 - by0)
                    crossings.append((bx0 + t * (bx1 - bx0), 1 if by1 > by0 else -1))
            if not crossings:
                continue
            crossings.sort()

            # 非零环绕规则:环绕数非零的横向区间算作字形内部。
            winding = 0
            start = None
            for x, direction in crossings:
                if winding == 0 and start is None:
                    start = x
                winding += direction
                if winding == 0 and start is not None:
                    first = max(0, math.ceil(start * SUPERSAMPLE - 0.5))
                    last = min(box_w * SUPERSAMPLE - 1,
                               math.ceil(x * SUPERSAMPLE - 0.5) - 1)
                    for sub in range(first, last + 1):
                        counts[sub // SUPERSAMPLE] += 1
                    start = None

        for px, hits in enumerate(counts):
            if hits:
                value = int(round(hits * 15 / total))
                nibbles[row * box_w + px] = value if value < 15 else 15
    return nibbles


def dump_glyph(cp: int, nibbles: Sequence[int], box_w: int, box_h: int,
               ofs_x: int, ofs_y: int) -> None:
    """把单个字形的覆盖度打印成 ASCII art,用于在没有设备时核对栅格化质量。"""
    print(f"U+{cp:04X} '{chr(cp)}' box={box_w}x{box_h} ofs=({ofs_x},{ofs_y})")
    for row in range(box_h):
        line = ""
        for px in range(box_w):
            value = nibbles[row * box_w + px]
            line += " " if value == 0 else ("." if value < 4 else ("+" if value < 9 else "#"))
        print("  |" + line + "|")


def pack_4bpp(nibbles: Sequence[int]) -> List[int]:
    """连续半字节流:每字节高半字节在前,最后不足一字节时低半字节补 0。"""
    out = []
    for i in range(0, len(nibbles), 2):
        high = nibbles[i]
        low = nibbles[i + 1] if i + 1 < len(nibbles) else 0
        out.append((high << 4) | low)
    return out


# ---------------------------------------------------------------------------
# 生成
# ---------------------------------------------------------------------------
def build_cmaps(codepoints: Sequence[int], first_glyph_id: int):
    """ASCII 用连续段,其余按稀疏段分组;返回 (表项, 稀疏列表, 码点顺序)。"""
    ascii_cps = [cp for cp in codepoints if ASCII_START <= cp <= ASCII_END]
    other_cps = [cp for cp in codepoints if cp not in set(ascii_cps)]

    cmaps = []
    sparse_lists = []
    ordered = []
    glyph_id = first_glyph_id

    if ascii_cps:
        cmaps.append(
            {
                "range_start": ASCII_START,
                "range_length": len(ascii_cps),
                "glyph_id_start": glyph_id,
                "list_name": None,
                "list_length": 0,
                "type": "LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY",
            }
        )
        ordered.extend(ascii_cps)
        glyph_id += len(ascii_cps)

    # 稀疏段:码点跨度不能超过 uint16 偏移上限,必要时拆成多段。
    group: List[int] = []
    for cp in other_cps:
        if not group:
            group = [cp]
            continue
        if cp - group[0] > 0xFFFE:
            cmaps.append(_sparse_cmap(group, glyph_id, len(sparse_lists)))
            sparse_lists.append(group)
            ordered.extend(group)
            glyph_id += len(group)
            group = [cp]
        else:
            group.append(cp)
    if group:
        cmaps.append(_sparse_cmap(group, glyph_id, len(sparse_lists)))
        sparse_lists.append(group)
        ordered.extend(group)

    return cmaps, sparse_lists, ordered


def _sparse_cmap(group: Sequence[int], glyph_id: int, index: int):
    return {
        "range_start": group[0],
        "range_length": group[-1] - group[0] + 1,
        "glyph_id_start": glyph_id,
        "list_name": f"unicode_list_{index + 1}",
        "list_length": len(group),
        "type": "LV_FONT_FMT_TXT_CMAP_SPARSE_TINY",
    }


def generate(args) -> int:
    font_path = pathlib.Path(args.font)
    if not font_path.is_file():
        raise SystemExit(f"找不到字体文件: {font_path}")

    sources = sorted(pathlib.Path("main").glob("*.c"))
    if not sources:
        raise SystemExit("未在 main/ 下找到源码,无法推导字符清单")
    codepoints = collect_codepoints(sources)
    if args.extra:
        codepoints.update(ord(c) for c in args.extra)

    font = TTFont(str(font_path), fontNumber=args.font_number)
    cmap = font.getBestCmap()
    missing = sorted(cp for cp in codepoints if cp not in cmap)
    if missing:
        detail = " ".join(f"{chr(cp)}(U+{cp:04X})" for cp in missing)
        raise SystemExit(f"字体缺少 {len(missing)} 个所需字形: {detail}")

    upem = font["head"].unitsPerEm
    size = args.size
    scale = size / upem
    # 行高与基线按子集实际墨迹计算(见 LEADING_PX),在字形循环里累加。
    ink_top = 0
    ink_bottom = 0

    hmtx = font["hmtx"]
    glyph_set = font.getGlyphSet()

    cmaps, sparse_lists, ordered = build_cmaps(sorted(codepoints), 1)
    if args.print_inventory:
        print(f"字符清单({len(ordered)} 个): " + "".join(chr(cp) for cp in ordered))

    # 收集位图与描述符(index 0 为保留项)
    bitmap: List[int] = []
    descs = [(0, 0, 0, 0, 0, 0)]
    for cp in ordered:
        name = cmap[cp]
        advance = hmtx[name][0]
        # 点对点模式下把步进取整到整数像素,保证每个字形都从整数像素列开始。
        adv_w = (round(advance * scale) * 16 if args.snap
                 else round(advance * scale * 16))
        if adv_w > 0xFFF:
            raise SystemExit(f"U+{cp:04X} 的 adv_w={adv_w} 超出 12 位上限")

        contours = glyph_contours(glyph_set, name)
        if not contours:
            box_w = box_h = ofs_x = ofs_y = 0
            nibbles: List[int] = []
        else:
            # 包围盒直接取自轮廓范围,保证栅格化时不会被裁剪。
            xs = [point[0] for contour in contours for point in contour]
            ys = [point[1] for contour in contours for point in contour]
            x0 = math.floor(min(xs) * scale)
            x1 = math.ceil(max(xs) * scale)
            y0 = math.floor(min(ys) * scale)
            y1 = math.ceil(max(ys) * scale)
            box_w, box_h = x1 - x0, y1 - y0
            ofs_x, ofs_y = x0, y0
            if box_w > 255 or box_h > 255 or not (-128 <= ofs_x <= 127) or not (-128 <= ofs_y <= 127):
                raise SystemExit(f"U+{cp:04X} 的盒/偏移超出 LVGL 字段范围")
            nibbles = rasterize(contours, scale, box_w, box_h, x0, y1)
            if args.snap:
                # 像素字体每个像素要么全亮要么全灭;阈值取 50%。
                nibbles = [15 if value >= 8 else 0 for value in nibbles]
            if args.dump_codepoint == cp:
                dump_glyph(cp, nibbles, box_w, box_h, ofs_x, ofs_y)

        ink_top = max(ink_top, box_h + ofs_y)
        ink_bottom = max(ink_bottom, -ofs_y)
        descs.append((len(bitmap), adv_w, box_w, box_h, ofs_x, ofs_y))
        bitmap.extend(pack_4bpp(nibbles))

    if len(bitmap) > 0xFFFFF:
        raise SystemExit("字形位图超过 1 MB,bitmap_index 字段放不下")

    line_height = ink_top + ink_bottom + LEADING_PX
    base_line = ink_bottom

    out_path = pathlib.Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    guard = out_path.stem.upper()

    with out_path.open("w", encoding="utf-8", newline="\n") as out:
        out.write(
            "/*******************************************************************************\n"
            f" * Size: {size} px\n"
            " * Bpp: 4\n"
            f" * Source: {font_path.name} ({font['name'].getDebugName(1) or 'unknown'})\n"
            " * Generator: tools/gen_cjk_font.py (fontTools + 内置扫描线栅格器)\n"
            " ******************************************************************************/\n\n"
            "#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n"
            '#include "lvgl.h"\n'
            "#else\n"
            '#include "lvgl.h"\n'
            "#endif\n\n"
            f"#ifndef {guard}\n#define {guard} 1\n#endif\n\n"
            f"#if {guard}\n\n"
            "/*-----------------\n *    BITMAPS\n *----------------*/\n\n"
            "static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {\n"
        )
        for index, cp in enumerate(ordered):
            desc_index = index + 1
            start = descs[desc_index][0]
            end = descs[desc_index + 1][0] if desc_index + 1 < len(descs) else len(bitmap)
            data = bitmap[start:end]
            if not data:
                continue   # 空字形(如空格)不占位图字节
            chars = repr(chr(cp)).replace("'", '"')
            out.write(f"    /* U+{cp:04X} {chars} */\n    ")
            out.write(", ".join(f"0x{b:02X}" for b in data))
            out.write(",\n\n")
        out.write("};\n\n")

        out.write(
            "/*---------------------\n *  GLYPH DESCRIPTIONS\n *---------------------*/\n\n"
            "static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {\n"
            "    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},\n"
        )
        for index, cp in enumerate(ordered):
            bitmap_index, adv_w, box_w, box_h, ofs_x, ofs_y = descs[index + 1]
            out.write(
                f"    {{.bitmap_index = {bitmap_index}, .adv_w = {adv_w}, "
                f".box_w = {box_w}, .box_h = {box_h}, .ofs_x = {ofs_x}, .ofs_y = {ofs_y}}},"
                f"   /* U+{cp:04X} */\n"
            )
        out.write("};\n\n")

        for values in sparse_lists:
            offsets = [str(cp - values[0]) for cp in values]
            name = f"unicode_list_{sparse_lists.index(values) + 1}"
            out.write(f"static const uint16_t {name}[] = {{\n    ")
            out.write(", ".join(offsets))
            out.write(",\n};\n\n")

        out.write("/*-----------------\n *  CHARACTER MAPPING\n *-----------------*/\n\n")
        out.write("static const lv_font_fmt_txt_cmap_t cmaps[] = {\n")
        for item in cmaps:
            unicode_list = item["list_name"] if item["list_name"] else "NULL"
            out.write(
                "    {\n"
                f"        .range_start = {item['range_start']}, .range_length = {item['range_length']}, "
                f".glyph_id_start = {item['glyph_id_start']},\n"
                f"        .unicode_list = {unicode_list}, .glyph_id_ofs_list = NULL, "
                f".list_length = {item['list_length']}, .type = {item['type']}\n"
                "    },\n"
            )
        out.write("};\n\n")

        out.write(
            "/*---------------------\n *  ALL CUSTOM DATA\n *---------------------*/\n\n"
            "#if LVGL_VERSION_MAJOR == 8\n"
            "static lv_font_fmt_txt_glyph_cache_t cache;\n"
            "#endif\n\n"
            "#if LVGL_VERSION_MAJOR >= 8\n"
            "static const lv_font_fmt_txt_dsc_t font_dsc = {\n"
            "#else\n"
            "static lv_font_fmt_txt_dsc_t font_dsc = {\n"
            "#endif\n"
            "    .glyph_bitmap = glyph_bitmap,\n"
            "    .glyph_dsc = glyph_dsc,\n"
            "    .cmaps = cmaps,\n"
            "    .kern_dsc = NULL,\n"
            "    .kern_scale = 0,\n"
            f"    .cmap_num = {len(cmaps)},\n"
            "    .bpp = 4,\n"
            "    .kern_classes = 0,\n"
            "    .bitmap_format = 0,\n"
            "#if LVGL_VERSION_MAJOR == 8\n"
            "    .cache = &cache\n"
            "#endif\n"
            "};\n\n"
            "/*-----------------\n *  PUBLIC FONT\n *----------------*/\n\n"
            "#if LVGL_VERSION_MAJOR >= 8\n"
            f"const lv_font_t {args.name} = {{\n"
            "#else\n"
            f"lv_font_t {args.name} = {{\n"
            "#endif\n"
            "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n"
            "    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n"
            f"    .line_height = {line_height},\n"
            f"    .base_line = {base_line},\n"
            "#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)\n"
            "    .subpx = LV_FONT_SUBPX_NONE,\n"
            "#endif\n"
            "#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8\n"
            "    .underline_position = 0,\n"
            "    .underline_thickness = 0,\n"
            "#endif\n"
            "    .dsc = &font_dsc,\n"
            "#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9\n"
            "    .fallback = NULL,\n"
            "#endif\n"
            "    .user_data = NULL,\n"
            "};\n\n"
            f"#endif /*#if {guard}*/\n"
        )

    print(
        f"{out_path}: {len(ordered)} 个字形, 位图 {len(bitmap)} 字节, "
        f"line_height={line_height}, base_line={base_line}, cmap={len(cmaps)} 段"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 LVGL 4bpp 中文子集字体")
    parser.add_argument("--font", required=True, help="源 TTF 路径")
    parser.add_argument("--size", type=int, required=True, help="字号(px)")
    parser.add_argument("--name", required=True, help="生成的 lv_font_t 符号名")
    parser.add_argument("--output", required=True, help="输出 .c 路径")
    parser.add_argument("--font-number", type=int, default=0, help="ttc 内的字体序号")
    parser.add_argument("--extra", default="", help="额外补入的字符")
    parser.add_argument("--print-inventory", action="store_true", help="打印字符清单")
    parser.add_argument(
        "--snap",
        action="store_true",
        help="像素字体点对点模式:覆盖度二值化为全开/全关,并把字宽取整到整数像素",
    )
    parser.add_argument(
        "--dump-codepoint",
        type=lambda value: int(value, 0),
        default=-1,
        help="打印单个字形的位图 ASCII art(如 0x4E2D)",
    )
    return generate(parser.parse_args())


if __name__ == "__main__":
    sys.exit(main())
