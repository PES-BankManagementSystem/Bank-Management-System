# Design Directory

This directory contains the Entity-Relationship (ER) diagram for the Bank Management System database.

## Files

- `er-diagram.mmd`: Mermaid syntax source file for the ER diagram
- `er-diagram.svg`: Exported SVG version of the ER diagram
- `er-diagram.png`: Exported PNG version of the ER diagram (if export was successful)

## Regenerating the Diagram

To regenerate the ER diagram from the Mermaid source:

### Using Mermaid CLI (Recommended)

1. Install the Mermaid CLI:
   ```bash
   npm install -g @mermaid-js/mermaid-cli
   ```

2. Generate the SVG:
   ```bash
   mmcd -i er-diagram.mmd -o er-diagram.svg
   ```

3. Generate the PNG (requires puppeteer or similar):
   ```bash
   mmcd -i er-diagram.mmd -o er-diagram.png
   ```

### Online Alternatives

If you don't want to install the CLI, you can use online Mermaid editors:
- https://mermaid.live/
- https://mermaidjs.github.io/mermaid-live-editor/

Simply paste the contents of `er-diagram.mmd` into the editor and export as SVG/PNG.

## Diagram Contents

The ER diagram shows:
- All tables in the database schema
- Primary keys (PK) and foreign keys (FK)
- Relationships between tables with cardinalities
- All columns with their data types

The diagram follows the schema defined in `db/schema.sql` and implements all relationships specified in the SRS Appendix B.