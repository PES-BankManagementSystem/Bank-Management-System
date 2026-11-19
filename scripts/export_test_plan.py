#!/usr/bin/env python3
"""Export the consolidated test-plan Markdown source to PDF."""

from __future__ import annotations

import html
import re
from pathlib import Path

import fitz
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs" / "TEST_PLAN_CONSOLIDATED.md"
PDF = ROOT / "docs" / "Test_Plan_BankManagementSystem.pdf"
W, H = A4
MARGIN = 18 * mm
WIDTH = W - 2 * MARGIN
INK = colors.HexColor("#18252f")
ACCENT = colors.HexColor("#27485d")


def inline(text: str) -> str:
    text = re.sub(r"<br\s*/?>", " ", text, flags=re.IGNORECASE)
    text = html.escape(text, quote=True)
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"`(.+?)`", r'<font name="Courier">\1</font>', text)
    return text


def split_table_row(line: str) -> list[str]:
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [cell.strip() for cell in line.split("|")]


def footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 8)
    canvas.setFillColor(colors.HexColor("#657681"))
    canvas.drawString(MARGIN, 12 * mm, "TP-BMS-2026-v1.2")
    canvas.drawRightString(W - MARGIN, 12 * mm, f"Page {doc.page}")
    canvas.restoreState()


def build_story():
    styles = getSampleStyleSheet()
    title = ParagraphStyle(
        "TitleCustom", parent=styles["Title"], fontName="Helvetica-Bold",
        fontSize=16, leading=19, textColor=INK, alignment=TA_CENTER,
        spaceAfter=8,
    )
    h1 = ParagraphStyle(
        "H1Custom", parent=styles["Heading1"], fontName="Helvetica-Bold",
        fontSize=12.5, leading=15, textColor=ACCENT, spaceBefore=12,
        spaceAfter=6,
    )
    h2 = ParagraphStyle(
        "H2Custom", parent=styles["Heading2"], fontName="Helvetica-Bold",
        fontSize=10.5, leading=13, textColor=INK, spaceBefore=9,
        spaceAfter=5,
    )
    body = ParagraphStyle(
        "BodyCustom", parent=styles["BodyText"], fontName="Helvetica",
        fontSize=8.5, leading=11, textColor=INK, alignment=TA_LEFT,
        spaceAfter=4,
    )
    bullet = ParagraphStyle(
        "BulletCustom", parent=body, leftIndent=12, firstLineIndent=-8,
        bulletIndent=4, spaceAfter=2,
    )
    table_header = ParagraphStyle(
        "TableHeader", parent=body, fontName="Helvetica-Bold",
        fontSize=7.3, leading=9, textColor=INK,
    )
    table_body = ParagraphStyle(
        "TableBody", parent=body, fontSize=7.1, leading=8.5,
        spaceAfter=0,
    )

    lines = SOURCE.read_text(encoding="utf-8").splitlines()
    story = []
    i = 0
    first_heading = True
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        if line.startswith("```"):
            i += 1
            continue
        if line.startswith("---"):
            i += 1
            continue

        if line.startswith("|") and i + 1 < len(lines) and re.match(r"^\|\s*:?-+", lines[i + 1]):
            headers = split_table_row(line)
            i += 2
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                rows.append(split_table_row(lines[i]))
                i += 1
            col_count = max([len(headers)] + [len(row) for row in rows])
            if col_count == 4:
                widths = [WIDTH * 0.13, WIDTH * 0.37, WIDTH * 0.22, WIDTH * 0.28]
            elif col_count == 3:
                widths = [WIDTH * 0.18, WIDTH * 0.42, WIDTH * 0.40]
            elif col_count == 2:
                widths = [WIDTH * 0.28, WIDTH * 0.72]
            else:
                widths = [WIDTH / col_count] * col_count

            table_rows = [[Paragraph(inline(cell), table_header) for cell in headers]]
            table_rows += [
                [Paragraph(inline(cell), table_body) for cell in row]
                for row in rows
            ]
            table = Table(table_rows, colWidths=widths, repeatRows=1, hAlign="LEFT")
            table.setStyle(TableStyle([
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e8eff3")),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1),
                 [colors.white, colors.HexColor("#f7f9fa")]),
                ("LINEBELOW", (0, 0), (-1, 0), 0.6, colors.HexColor("#b8c7cf")),
                ("LINEBELOW", (0, 1), (-1, -1), 0.25, colors.HexColor("#dbe2e7")),
                ("LEFTPADDING", (0, 0), (-1, -1), 5),
                ("RIGHTPADDING", (0, 0), (-1, -1), 5),
                ("TOPPADDING", (0, 0), (-1, -1), 5),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
            ]))
            story.extend([table, Spacer(1, 7)])
            continue

        if line.startswith("# "):
            text = line[2:].strip()
            story.append(Paragraph(inline(text), title if first_heading else h1))
            first_heading = False
            i += 1
            continue
        if line.startswith("## "):
            story.append(Paragraph(inline(line[3:].strip()), h1))
            i += 1
            continue
        if line.startswith("### "):
            story.append(Paragraph(inline(line[4:].strip()), h2))
            i += 1
            continue
        if line.startswith("- ") or re.match(r"^\d+\.\s+", line):
            marker = ""
            text = re.sub(r"^-\s+", "", line)
            text = re.sub(r"^(\d+)\.\s+", r"\1. ", text)
            if text.startswith(marker):
                pass
            story.append(Paragraph(inline(text), bullet, bulletText="•"))
            i += 1
            continue
        if line.startswith("> "):
            story.append(Paragraph(inline(line[2:].strip()), body))
            i += 1
            continue

        paragraph = [line]
        i += 1
        while i < len(lines):
            next_line = lines[i].strip()
            if (
                not next_line
                or next_line.startswith("#")
                or next_line.startswith("|")
                or next_line.startswith("- ")
                or next_line.startswith("> ")
                or re.match(r"^\d+\.\s+", next_line)
                or next_line.startswith("---")
            ):
                break
            paragraph.append(next_line)
            i += 1
        story.append(Paragraph(inline(" ".join(paragraph)), body))

    return story


def main():
    document = BaseDocTemplate(
        str(PDF),
        pagesize=A4,
        leftMargin=MARGIN,
        rightMargin=MARGIN,
        topMargin=18 * mm,
        bottomMargin=17 * mm,
        title="Test Plan & Consolidated Test Cases v1.2",
        author="Bank Management System QA Team",
        subject="Bank Management System consolidated test plan",
        pageCompression=1,
        invariant=1,
    )
    frame = Frame(
        MARGIN,
        17 * mm,
        WIDTH,
        H - 35 * mm,
        leftPadding=0,
        rightPadding=0,
        topPadding=0,
        bottomPadding=0,
    )
    document.addPageTemplates(
        PageTemplate(id="TestPlan", frames=[frame], onPage=footer)
    )
    document.build(build_story())


if __name__ == "__main__":
    main()
