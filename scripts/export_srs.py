#!/usr/bin/env python3
"""Export the editable SRS to PDF and regenerate its schema-backed diagrams.

Run from any directory: python scripts/export_srs.py [--qa-dir TEMP_DIRECTORY]
Dependencies: reportlab, Markdown, PyMuPDF (tested with 5.1.0, 3.11.3, 1.26.7).
The SQLite schema is inspected in memory; no database file is created.
"""
from __future__ import annotations

import argparse
import hashlib
import html
import io
import json
import re
import sqlite3
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

import fitz
import markdown
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.platypus import (
    BaseDocTemplate, Frame, Image, KeepTogether, PageBreak, PageTemplate,
    Paragraph, Spacer, Table, TableStyle,
)

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / 'docs'
SOURCE = DOCS / 'SRS_Bank_Management_System.md'
PDF = DOCS / 'SRS_Bank_Management_System.pdf'
W, H = A4
MARGIN = 43
WIDTH = W - 2 * MARGIN
INK = colors.HexColor('#18252f')
ACCENT = colors.HexColor('#27485d')


def esc(text):
    return html.escape(str(text), quote=True)


def svg_text(x, y, text, size=17, weight='normal', anchor='start'):
    return (f'<text x="{x}" y="{y}" font-size="{size}" font-weight="{weight}" '
            f'text-anchor="{anchor}" fill="#18252f">{esc(text)}</text>')


def svg_start(w, h):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" '
            f'viewBox="0 0 {w} {h}"><rect width="{w}" height="{h}" fill="white"/>'
            '<g font-family="Arial, Helvetica, sans-serif">')


def diagram_use_cases():
    parts = [svg_start(1000, 900)]
    parts.append('<rect x="252" y="28" width="732" height="850" rx="8" fill="#fafcfd" stroke="#27485d" stroke-width="2"/>')
    parts.append(svg_text(618, 59, 'Bank Management System', 23, 'bold', 'middle'))
    groups = [
        ('All three roles', 165, [('Sign in and first', 'credential change'), ('Sign out current', 'session'), ('View role', 'dashboard')]),
        ('Administrator', 375, [('Manage staff and', 'unlock users'), ('Configure types and', 'deactivate customers'), ('Run interest and', 'browse audit trail')]),
        ('Admin and Officer', 585, [('Register, search and', 'edit customers'), ('Open/manage accounts;', 'deposit/withdraw/transfer'), ('Read any account', 'and statement')]),
        ('Customer', 795, [('Read own profile', 'and accounts'), ('Read own balance', 'and history'), ('Print/export own', 'statements')]),
    ]
    for label, y, cases in groups:
        parts.append(f'<circle cx="107" cy="{y-39}" r="19" fill="white" stroke="#18252f" stroke-width="3"/>')
        parts.append(f'<path d="M107 {y-20} V{y+24} M76 {y-3} H138 M107 {y+24} L82 {y+57} M107 {y+24} L132 {y+57}" stroke="#18252f" stroke-width="3" fill="none"/>')
        parts.append(svg_text(107, y+88, label, 20, 'bold', 'middle'))
        for j, (a, b) in enumerate(cases):
            cy = y-53+j*56
            parts.append(f'<path d="M138 {y-3} L302 {cy}" stroke="#7c8b95" fill="none" stroke-width="1.4"/>')
            parts.append(f'<ellipse cx="620" cy="{cy}" rx="318" ry="25" fill="white" stroke="#516978" stroke-width="1.5"/>')
            parts.append(svg_text(620, cy-3, a+' '+b, 18, anchor='middle'))
    # Three actual actors; the first row is a shared-use-case legend, not a
    # fourth human role. Administrator also inherits the shared staff cases.
    joined='\n'.join(parts)
    shared_actor=re.compile(r'<circle cx="107" cy="126".*?(?=<path d="M138 162)', re.S)
    joined=shared_actor.sub(svg_text(107,160,'All roles',20,'bold','middle')+'\n',joined)
    joined=joined.replace('Admin and Officer','Bank Officer')
    parts=[joined,svg_text(620,686,'Shared staff cases also permit Administrator',16,anchor='middle')]
    parts.append('</g></svg>')
    return '\n'.join(parts)


def diagram_er(connection):
    names = [r[0] for r in connection.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")]
    assert set(names) == {'users', 'customers', 'account_types', 'accounts', 'transactions', 'audit_log', 'interest_runs'}
    assert connection.execute('PRAGMA user_version').fetchone()[0] == 5
    assert not any(r[3] == 'interest_run_id' for r in connection.execute('PRAGMA foreign_key_list(transactions)'))
    nodes = {
        'users': (25, 30, ['user_id PK', 'username UNIQUE; role', 'status; failed_attempts', 'password_hash; must_change_password']),
        'customers': (555, 30, ['customer_id PK; customer_code UNIQUE', 'user_id nullable UNIQUE FK', 'status; name/contact/identity', 'date_of_birth']),
        'account_types': (25, 280, ['type_id PK; type_name UNIQUE', 'min_balance; interest_rate', 'daily_withdrawal_limit']),
        'accounts': (555, 280, ['account_id PK; account_number UNIQUE', 'customer_id FK; type_id FK', 'status; balance']),
        'audit_log': (25, 530, ['log_id PK; user_id FK', 'action; entity; entity_id', 'details; timestamp', 'entity_id is polymorphic, no account FK']),
        'transactions': (555, 530, ['transaction_id PK', 'account_id FK; operator_id FK', 'type; amount; resulting_balance', 'transfer_ref; interest_run_id (no FK)']),
        'interest_runs': (25, 780, ['run_id PK; period UNIQUE', 'run_by FK; run_at', 'accounts_credited; total_interest']),
    }
    # Validate every displayed attribute against the schema; the summary lines
    # intentionally omit non-key details and timestamps to keep the figure legible.
    expected = {
        'users': ['user_id','username','role','status','failed_attempts','password_hash','must_change_password'],
        'customers': ['customer_id','customer_code','user_id','status','date_of_birth'],
        'account_types': ['type_id','type_name','min_balance','interest_rate','daily_withdrawal_limit'],
        'accounts': ['account_id','account_number','customer_id','type_id','status','balance'],
        'transactions': ['transaction_id','account_id','operator_id','type','amount','resulting_balance','transfer_ref','interest_run_id'],
        'audit_log': ['log_id','user_id','action','entity','entity_id','details','timestamp'],
        'interest_runs': ['run_id','period','run_by','run_at','accounts_credited','total_interest'],
    }
    for name, columns in expected.items():
        assert set(columns) <= {r[1] for r in connection.execute(f'PRAGMA table_info({name})')}
    p = [svg_start(1020, 1010)]
    # The relationships are shown with labeled paths; the exact nullable and
    # logical constraints are also tabulated in SRS Appendix B.2.
    edges = [
        ('M455 110 H555', 500, 88, '0..1 : 0..1', False),
        ('M765 210 V280', 855, 255, '1 : 0..many', False),
        ('M455 355 H555', 505, 334, '1 : 0..many', False),
        ('M765 460 V530', 855, 506, '1 : 0..many', False),
        ('M52 210 V530', 154, 495, 'user 1 : 0..many audit', False),
        ('M455 175 H480 V573 H555', 495, 478, 'operator', False),
        ('M255 210 V248 H8 V855 H25', 110, 752, 'user 1 : 0..many runs', False),
        ('M455 855 H480 M490 855 H515 M525 855 H550 M560 855 H585 M595 855 H620 M630 855 H655 M665 855 H690 M700 855 H725 M735 855 H765 V830 M765 820 V795 M765 785 V760 M765 750 V725 M765 715 V710', 747, 776, 'logical 1 : 0..many', True),
    ]
    for path, x, y, label, logical in edges:
        dash = ' stroke-dasharray="8 5"' if logical else ''
        p.append(f'<path d="{path}" stroke="#526b79" stroke-width="2" fill="none"{dash}/>')
        p.append(svg_text(x, y, label, 16, anchor='middle'))
    for name, (x, y, lines) in nodes.items():
        p.append(f'<rect x="{x}" y="{y}" width="430" height="180" rx="5" fill="white" stroke="#27485d" stroke-width="2"/>')
        p.append(f'<rect x="{x+1}" y="{y+1}" width="428" height="35" fill="#e8eff3"/>')
        p.append(svg_text(x+14, y+26, name, 22, 'bold'))
        for j, line in enumerate(lines):
            p.append(svg_text(x+14, y+63+j*29, line, 17))
    p.append(svg_text(550, 965, 'Solid: declared FK. Dashed: logical link only.', 18))
    p.append(svg_text(550, 991, 'Amounts: paise. Rate: basis points. Schema v5.', 17))
    p.append('</g></svg>')
    return '\n'.join(p)


def write_diagrams():
    directory = DOCS / 'diagrams'
    directory.mkdir(exist_ok=True)
    conn = sqlite3.connect(':memory:')
    conn.executescript((ROOT / 'db/schema.sql').read_text(encoding='utf-8'))
    diagrams = {'srs-v1.2-use-cases.svg': diagram_use_cases(), 'srs-v1.2-er.svg': diagram_er(conn)}
    conn.close()
    for name, data in diagrams.items():
        (directory / name).write_text(data+'\n', encoding='utf-8')


def inline(element):
    chunks = [esc(element.text or '')]
    for child in element:
        tag = child.tag
        value = inline(child)
        if tag in ('strong', 'b'):
            chunks.append('<b>'+value+'</b>')
        elif tag in ('em', 'i'):
            chunks.append('<i>'+value+'</i>')
        elif tag == 'code':
            chunks.append('<font name="Courier" size="8">'+value+'</font>')
        elif tag == 'a':
            chunks.append(value)  # Reference paths remain visible and extractable.
        elif tag == 'br':
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
    canvas.setFillColor(colors.HexColor('#53616c'))
    canvas.drawString(MARGIN, 25, 'Bank Management System | SRS v1.2 | 6 November 2026')
    canvas.drawRightString(W-MARGIN, 25, str(document.page))
    canvas.restoreState()


def build_pdf(qa_dir):
    source = SOURCE.read_text(encoding='utf-8')
    assert '{{' not in source, 'Unexpanded source placeholder'
    source=source.replace('<!-- pagebreak -->','<div class="pagebreak"></div>')
    root = ET.fromstring('<root>'+markdown.markdown(source, extensions=['tables'])+'</root>')
    base = getSampleStyleSheet()
    body = ParagraphStyle('SRSBody', fontName='Helvetica', fontSize=10.1, leading=14.4, textColor=INK, spaceAfter=8, splitLongWords=True)
    table_text = ParagraphStyle('SRSTable', parent=body, fontSize=8.5, leading=11.4, spaceAfter=0)
    table_head = ParagraphStyle('SRSTableHead', parent=table_text, fontName='Helvetica-Bold', textColor=ACCENT)
    styles = {
        'h1': ParagraphStyle('SRSTitle', parent=body, fontName='Helvetica-Bold', fontSize=28, leading=34, spaceBefore=66, spaceAfter=22),
        'h2': ParagraphStyle('SRSHeading1', parent=body, fontName='Helvetica-Bold', fontSize=18, leading=23, textColor=ACCENT, spaceBefore=15, spaceAfter=12, keepWithNext=True),
        'h3': ParagraphStyle('SRSHeading2', parent=body, fontName='Helvetica-Bold', fontSize=12.7, leading=17, spaceBefore=13, spaceAfter=7, keepWithNext=True),
        'h4': ParagraphStyle('SRSHeading3', parent=body, fontName='Helvetica-Bold', fontSize=10.8, leading=15, spaceBefore=10, spaceAfter=6, keepWithNext=True),
    }
    story = []
    headings = []
    diagrams_seen = []
    for element in root:
        tag = element.tag
        text = ''.join(element.itertext())
        if tag == 'div' and element.attrib.get('class') == 'pagebreak':
            story.append(PageBreak())
        elif tag in styles:
            if tag == 'h2' and re.match(r'^(?:[1-6]\. |Appendix [A-F]\.)', text):
                story.append(PageBreak())
            headings.append(text)
            story.append(Paragraph(inline(element), styles[tag]))
        elif tag == 'p' and element.find('img') is not None:
            img = element.find('img')
            path = DOCS / img.attrib['src']
            svg = fitz.open(str(path))
            pix = svg[0].get_pixmap(matrix=fitz.Matrix(2, 2))
            png = io.BytesIO(pix.tobytes('png'))
            ratio = pix.height/pix.width
            width = WIDTH
            height = width*ratio
            if height > 545:
                height = 545
                width = height/ratio
            picture = Image(png, width=width, height=height)
            picture.hAlign = 'CENTER'
            caption = Paragraph(esc(img.attrib.get('alt','')), ParagraphStyle('Caption', parent=body, fontSize=9, leading=12, alignment=TA_CENTER))
            story.append(KeepTogether([picture, Spacer(1, 5), caption]))
            diagrams_seen.append(str(path.relative_to(ROOT)))
            svg.close()
        elif tag == 'p':
            story.append(Paragraph(inline(element), body))
        elif tag in ('ul','ol'):
            for idx, li in enumerate(element):
                prefix = f'{idx+1}. ' if tag == 'ol' else '\u2022 '
                story.append(Paragraph(prefix+inline(li), ParagraphStyle('List', parent=body, leftIndent=10, firstLineIndent=-8, spaceAfter=5)))
        elif tag == 'table':
            rows = []
            for section in element:
                for tr in section:
                    rows.append([Paragraph(inline(cell), table_head if section.tag == 'thead' else table_text) for cell in tr])
            n = len(rows[0])
            if n == 5:
                widths = [38, 135, 88, 74, WIDTH-335]
            elif n == 4:
                widths = [52, 74, 102, WIDTH-228]
            elif n == 3:
                widths = [45, WIDTH*.43, WIDTH-45-WIDTH*.43]
            else:
                first_header=''.join(element.find('thead')[0][0].itertext())
                first_width={'ID':48, 'Reference':65, 'Term':130}.get(first_header, WIDTH*.27)
                widths = [first_width, WIDTH-first_width]
            table = Table(rows, colWidths=widths, repeatRows=1, hAlign='LEFT')
            table.setStyle(TableStyle([
                ('VALIGN',(0,0),(-1,-1),'TOP'),
                ('BACKGROUND',(0,0),(-1,0),colors.HexColor('#e8eff3')),
                ('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white, colors.HexColor('#f7f9fa')]),
                ('LINEBELOW',(0,0),(-1,0),.6,colors.HexColor('#b8c7cf')),
                ('LINEBELOW',(0,1),(-1,-1),.25,colors.HexColor('#dbe2e7')),
                ('LEFTPADDING',(0,0),(-1,-1),6),('RIGHTPADDING',(0,0),(-1,-1),6),
                ('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),
            ]))
            story += [table, Spacer(1, 10)]
    document = BaseDocTemplate(str(PDF), pagesize=A4, leftMargin=MARGIN, rightMargin=MARGIN, topMargin=40, bottomMargin=43, title='Bank Management System Software Requirements Specification v1.2', author='Bank Management System project team', pageCompression=1, invariant=1)
    frame = Frame(MARGIN, 43, WIDTH, H-83, leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    document.addPageTemplates(PageTemplate(id='SRS', frames=[frame], onPage=footer))
    document.build(story)
    qa_dir.mkdir(parents=True, exist_ok=True)
    pdf = fitz.open(str(PDF))
    texts = [page.get_text() for page in pdf]
    all_text = '\n'.join(texts)
    required_ids = [f'FR-{i}' for i in range(1,18)]+[f'NFR-{i}' for i in range(1,10)]+[f'SEC-{i}' for i in range(1,7)]+[f'SO-{i}' for i in range(1,4)]+[f'BR-{i}' for i in range(1,11)]
    missing = [s for s in required_ids if not re.search(r'(?<![A-Z0-9-])'+re.escape(s)+r'(?!\d)',all_text)]
    # Text line wrapping can split a heading across lines; normalize whitespace.
    normalized = re.sub(r'\s+',' ',all_text)
    missing_headings = [s for s in headings if re.sub(r'\s+',' ',s) not in normalized]
    outside=[]
    for page_no, page in enumerate(pdf,1):
        for block in page.get_text('dict')['blocks']:
            if block.get('type') != 0:continue
            x0,y0,x1,y1=block['bbox']
            if x0 < MARGIN-2 or x1 > W-MARGIN+2 or y0 < 20 or y1 > H-15:
                outside.append({'page':page_no,'bbox':[round(v,1) for v in block['bbox']]})
        page.get_pixmap(matrix=fitz.Matrix(1.5,1.5)).save(str(qa_dir/f'page-{page_no:02}.png'))
    manifest = {
        'source': str(SOURCE.relative_to(ROOT)), 'pdf': str(PDF.relative_to(ROOT)),
        'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
        'pdf_sha256':hashlib.sha256(PDF.read_bytes()).hexdigest(),
        'page_count':len(pdf), 'text_characters':len(all_text),
        'headings_checked':len(headings), 'missing_headings':missing_headings,
        'required_ids_checked':len(required_ids), 'missing_ids':missing,
        'diagram_sources':diagrams_seen,
        'embedded_image_count':sum(len(page.get_images()) for page in pdf),
        'text_blocks_outside_page_content_bounds':outside,
        'revision_v1_2_present':'1.2' in all_text and '6 Nov 2026' in all_text,
        'schema_version':5,
        'schema_fk_note':'transactions.interest_run_id is logical, not a declared foreign key',
        'visual_review':'PENDING; inspect every rendered page before release',
    }
    (qa_dir/'extracted.txt').write_text(all_text,encoding='utf-8')
    (qa_dir/'verification.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest,indent=2))
    pdf.close()
    assert not missing and not missing_headings and not outside
    assert len(diagrams_seen)==2 and manifest['embedded_image_count']>=2


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--qa-dir',type=Path,default=Path(tempfile.gettempdir())/'bms-srs-v1.2-qa')
    args=parser.parse_args()
    write_diagrams()
    build_pdf(args.qa_dir)


if __name__ == '__main__':
    main()
