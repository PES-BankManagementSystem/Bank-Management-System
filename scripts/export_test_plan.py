#!/usr/bin/env python3
"""Export the Test Plan Markdown source to its PDF and check the result.

Run from any directory: python scripts/export_test_plan.py [--qa-dir DIRECTORY]
Dependencies: reportlab, Markdown, PyMuPDF (the same as scripts/export_srs.py).
The QA directory receives a PNG of every page and the extracted text for review.
"""
from __future__ import annotations

import argparse
import html
import json
import re
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

import fitz
import markdown
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (
    BaseDocTemplate, Frame, KeepTogether, PageBreak, PageTemplate, Paragraph, Spacer, Table,
    TableStyle,
)

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / 'docs'
SOURCE = DOCS / 'TEST_PLAN_CONSOLIDATED.md'
PDF = DOCS / 'Test_Plan_BankManagementSystem.pdf'
W, H = A4
MARGIN = 43
WIDTH = W - 2 * MARGIN
INK = colors.HexColor('#18252f')
ACCENT = colors.HexColor('#27485d')
MUTED = colors.HexColor('#53616c')

# Relative column widths, chosen by the table's header row.
COLUMNS = {
    ('Version', 'Date', 'Author', 'Changes'): [9, 11, 17, 63],
    ('Reference', 'Document'): [11, 89],
    ('Field', 'Value'): [24, 76],
    ('Test item', 'Location', 'Description'): [28, 22, 50],
    ('Requirement(s)', 'Feature', 'Test cases'): [26, 42, 32],
    ('Suite ID', 'Suite name', 'Description', 'Test cases'): [9, 21, 42, 28],
    ('Name', 'Responsibility'): [28, 72],
    ('Activity', 'Dates', 'Status'): [50, 18, 32],
    ('Risk', 'Contingency'): [38, 62],
    ('Role', 'Name', 'Signature', 'Date'): [24, 32, 28, 16],
    ('Requirement', 'Summary', 'Test cases', 'S-43 status and coverage notes'): [14, 28, 24, 34],
}


def esc(text):
    return html.escape(str(text), quote=False)


def inline(element):
    """Convert one Markdown HTML element's children to reportlab markup."""
    chunks = [esc(element.text or '')]
    for child in element:
        value = inline(child)
        if child.tag in ('strong', 'b'):
            chunks.append('<b>' + value + '</b>')
        elif child.tag in ('em', 'i'):
            chunks.append('<i>' + value + '</i>')
        elif child.tag == 'code':
            chunks.append('<font name="Courier" size="8.4">' + value + '</font>')
        elif child.tag == 'br':
            chunks.append('<br/>')
        else:
            chunks.append(value)
        chunks.append(esc(child.tail or ''))
    return ''.join(chunks)


def footer(canvas, document):
    if document.page == 1:
        return
    canvas.saveState()
    canvas.setFont('Helvetica', 8)
    canvas.setFillColor(MUTED)
    canvas.drawString(MARGIN, 25, document.footer_text)
    canvas.drawRightString(W - MARGIN, 25, f'Page {document.page}')
    canvas.restoreState()


def make_table(element, text_style, head_style):
    rows = []
    for section in element:
        for tr in section:
            style = head_style if section.tag == 'thead' else text_style
            rows.append([Paragraph(inline(cell), style) for cell in tr])
    header = tuple(''.join(cell.itertext()).strip() for cell in element.find('thead')[0])
    weights = COLUMNS.get(header, [1] * len(header))
    widths = [WIDTH * w / sum(weights) for w in weights]
    table = Table(rows, colWidths=widths, repeatRows=1, hAlign='LEFT')
    table.setStyle(TableStyle([
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e8eff3')),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.HexColor('#f7f9fa')]),
        ('LINEBELOW', (0, 0), (-1, 0), .6, colors.HexColor('#b8c7cf')),
        ('LINEBELOW', (0, 1), (-1, -1), .25, colors.HexColor('#dbe2e7')),
        ('LEFTPADDING', (0, 0), (-1, -1), 5), ('RIGHTPADDING', (0, 0), (-1, -1), 5),
        ('TOPPADDING', (0, 0), (-1, -1), 5), ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
    ]))
    return table


def build_story(source):
    source = source.replace('<!-- pagebreak -->', '<div class="pagebreak"></div>')
    root = ET.fromstring('<root>' + markdown.markdown(source, extensions=['tables']) + '</root>')
    body = ParagraphStyle('Body', fontName='Helvetica', fontSize=9.6, leading=13.4,
                          textColor=INK, spaceAfter=6, splitLongWords=True)
    styles = {
        'title': ParagraphStyle('Title', parent=body, fontName='Helvetica-Bold', fontSize=26,
                                leading=32, alignment=TA_CENTER, spaceBefore=120, spaceAfter=18),
        'subtitle': ParagraphStyle('Subtitle', parent=body, fontSize=11, leading=16,
                                   alignment=TA_CENTER, spaceAfter=8),
        'h2': ParagraphStyle('H2', parent=body, fontName='Helvetica-Bold', fontSize=16, leading=21,
                             textColor=ACCENT, spaceBefore=14, spaceAfter=9, keepWithNext=True),
        'h3': ParagraphStyle('H3', parent=body, fontName='Helvetica-Bold', fontSize=12.2,
                             leading=16, spaceBefore=11, spaceAfter=6, keepWithNext=True),
        'h4': ParagraphStyle('H4', parent=body, fontName='Helvetica-Bold', fontSize=10.6,
                             leading=14, textColor=ACCENT, spaceBefore=12, spaceAfter=5,
                             keepWithNext=True),
        'list': ParagraphStyle('List', parent=body, leftIndent=14, firstLineIndent=-10, spaceAfter=3),
        'cell': ParagraphStyle('Cell', parent=body, fontSize=8.4, leading=11, spaceAfter=0),
    }
    styles['head'] = ParagraphStyle('Head', parent=styles['cell'], fontName='Helvetica-Bold',
                                    textColor=ACCENT)
    story, headings, title_block = [], [], True
    case = []  # flowables of the current test case, kept together on one page

    def close_case():
        if case:
            story.append(KeepTogether(case[:]))
            case.clear()

    for element in root:
        tag = element.tag
        text = ''.join(element.itertext())
        if tag in ('div', 'h1', 'h2', 'h3', 'h4'):
            close_case()
        out = case if (case or tag == 'h4') else story
        if tag == 'div' and element.attrib.get('class') == 'pagebreak':
            story.append(PageBreak())
        elif tag == 'h1':
            headings.append(text)
            story.append(Paragraph(inline(element), styles['title']))
        elif tag in ('h2', 'h3', 'h4'):
            if tag == 'h2':
                title_block = False
            headings.append(text)
            out.append(Paragraph(inline(element), styles[tag]))
        elif tag == 'p':
            out.append(Paragraph(inline(element), styles['subtitle'] if title_block else body))
        elif tag in ('ul', 'ol'):
            for index, item in enumerate(element, 1):
                marker = f'{index}.' if tag == 'ol' else '•'
                out.append(Paragraph(f'{marker}&nbsp;&nbsp;' + inline(item), styles['list']))
            out.append(Spacer(1, 4))
        elif tag == 'table':
            out.extend([make_table(element, styles['cell'], styles['head']), Spacer(1, 8)])
        else:
            raise ValueError(f'Unsupported Markdown element <{tag}>: {text[:60]!r}')
    close_case()
    return story, headings


def check_source(source):
    # The built-in Helvetica font covers Windows-1252 only; anything else would
    # render as a missing glyph.
    bad = sorted({ch for ch in source if ch not in '\n' and not ch.encode('cp1252', 'ignore')})
    assert not bad, f'Characters the PDF font cannot show: {bad}'
    version = re.search(r'^Version (\d+\.\d+) \| ([^|]+?) \|', source, re.M)
    assert version, 'Title block must contain "Version X.Y | <date> |"'
    return version.group(1), version.group(2).strip()


def verify(source, headings, qa_dir):
    pdf = fitz.open(str(PDF))
    texts = [page.get_text() for page in pdf]
    flat = re.sub(r'\s+', ' ', '\n'.join(texts))
    cases = sorted(set(re.findall(r'^#### (TC-\d+[A-Z]?):', source, re.M)))
    outside = []
    qa_dir.mkdir(parents=True, exist_ok=True)
    for number, page in enumerate(pdf, 1):
        for block in page.get_text('dict')['blocks']:
            x0, y0, x1, y1 = block['bbox']
            if block.get('type') == 0 and (x0 < MARGIN - 2 or x1 > W - MARGIN + 2 or y0 < 20 or y1 > H - 15):
                outside.append({'page': number, 'bbox': [round(v, 1) for v in block['bbox']]})
        page.get_pixmap(matrix=fitz.Matrix(1.5, 1.5)).save(str(qa_dir / f'page-{number:02}.png'))
    (qa_dir / 'extracted.txt').write_text('\n'.join(texts), encoding='utf-8')
    report = {
        'source': str(SOURCE.relative_to(ROOT)),
        'pdf': str(PDF.relative_to(ROOT)),
        'page_count': len(pdf),
        'headings_checked': len(headings),
        'missing_headings': [h for h in headings if re.sub(r'\s+', ' ', h) not in flat],
        'test_cases_checked': len(cases),
        'missing_test_cases': [c for c in cases if c + ':' not in flat],
        'literal_html_tags': sorted(set(re.findall(r'</?(?:br|p|div|b|i)\s*/?>', flat))),
        'text_outside_page_bounds': outside,
        'qa_dir': str(qa_dir),
    }
    pdf.close()
    print(json.dumps(report, indent=2))
    assert len(cases) == 26, f'Expected 26 test cases, found {len(cases)}'
    assert not (report['missing_headings'] or report['missing_test_cases']
                or report['literal_html_tags'] or outside), 'PDF verification failed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qa-dir', type=Path,
                        default=Path(tempfile.gettempdir()) / 'bms-test-plan-qa')
    args = parser.parse_args()
    source = SOURCE.read_text(encoding='utf-8')
    version, date = check_source(source)
    story, headings = build_story(source)
    document = BaseDocTemplate(
        str(PDF), pagesize=A4, leftMargin=MARGIN, rightMargin=MARGIN, topMargin=40,
        bottomMargin=43, title=f'Bank Management System Software Test Plan v{version}',
        author='Bank Management System project team', pageCompression=1, invariant=1)
    document.footer_text = f'Bank Management System | Software Test Plan v{version} | {date}'
    frame = Frame(MARGIN, 43, WIDTH, H - 83, leftPadding=0, rightPadding=0, topPadding=0,
                  bottomPadding=0)
    document.addPageTemplates(PageTemplate(id='TestPlan', frames=[frame], onPage=footer))
    document.build(story)
    verify(source, headings, args.qa_dir)


if __name__ == '__main__':
    main()
