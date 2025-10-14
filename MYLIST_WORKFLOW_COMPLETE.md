# MyList Import - Complete Workflow

## Overview
This document shows the complete workflow for importing MyList data using all available methods.

```
╔══════════════════════════════════════════════════════════════════╗
║                    MYLIST IMPORT OPTIONS                         ║
╚══════════════════════════════════════════════════════════════════╝

┌──────────────────────────────────────────────────────────────────┐
│  1. Fetch MyList Stats from API (UDP)                           │
│     Purpose: Get statistics only (count, watched, etc.)          │
│     ⚠️  Does not import actual entries                           │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ Shows statistics │
                    └─────────────────┘


┌──────────────────────────────────────────────────────────────────┐
│  2. Download MyList from AniDB (NEW - RECOMMENDED)               │
│     Purpose: Streamlined download with template selection        │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                ┌─────────────────────────────┐
                │ Template Selection Dialog   │
                │                             │
                │ ○ csv-adborg (Recommended)  │
                │ ○ xml (Full data)           │
                │ ○ csv (Standard)            │
                │ ○ json (JSON format)        │
                │ ○ anidb (Default)           │
                └─────────────────────────────┘
                              │
                              ▼
                ┌─────────────────────────────┐
                │ Browser opens to:           │
                │ anidb.net/user/export       │
                │   ?template=csv-adborg      │
                │   &list=mylist              │
                └─────────────────────────────┘
                              │
                              ▼
                ┌─────────────────────────────┐
                │ User logs in (if needed)    │
                │ Clicks "Export" button      │
                │ Downloads file              │
                └─────────────────────────────┘
                              │
                              ▼
                ┌─────────────────────────────┐
                │ Return to Usagi-dono        │
                │ Click "Import from File"    │
                └─────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ Data imported!  │
                    └─────────────────┘


┌──────────────────────────────────────────────────────────────────┐
│  3. Import MyList from File (Manual)                             │
│     Purpose: Import previously downloaded export file            │
└──────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                ┌─────────────────────────────┐
                │ User manually downloads     │
                │ from anidb.net/user/export  │
                └─────────────────────────────┘
                              │
                              ▼
                ┌─────────────────────────────┐
                │ Click "Import from File"    │
                │ Select downloaded file      │
                └─────────────────────────────┘
                              │
                              ▼
                ┌─────────────────────────────┐
                │ Auto-detect format:         │
                │ - XML (mylistexport)        │
                │ - CSV (header-based)        │
                │   • csv-adborg              │
                │   • standard csv            │
                │   • custom templates        │
                │ - TXT (fallback)            │
                └─────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │ Data imported!  │
                    └─────────────────┘


╔══════════════════════════════════════════════════════════════════╗
║                      FORMAT SUPPORT                              ║
╚══════════════════════════════════════════════════════════════════╝

XML Format:
  <mylistexport>
    <mylist lid="..." fid="..." eid="..." aid="..." />
  </mylistexport>

CSV Standard Format:
  lid,fid,eid,aid,gid,date,state,viewdate,storage,...

CSV csv-adborg Format:
  aid,eid,gid,lid,status,viewdate,anime_name,episode_name

CSV Custom Format (Any template with header):
  {any_column_order} - Auto-detected via header parsing


╔══════════════════════════════════════════════════════════════════╗
║                    TEMPLATE SUPPORT                              ║
╚══════════════════════════════════════════════════════════════════╝

Supported Templates:
┌────────────────┬──────────────────────────────────────────────┐
│ Template       │ Description                                  │
├────────────────┼──────────────────────────────────────────────┤
│ csv-adborg     │ Recommended - Optimized field order          │
│ xml            │ Full structured data in XML format           │
│ csv            │ Standard comma-separated values              │
│ json           │ JSON format for modern applications          │
│ anidb          │ AniDB's default template format              │
│ custom         │ Any template with header row                 │
└────────────────┴──────────────────────────────────────────────┘

All CSV templates supported through intelligent header-based parsing.


╔══════════════════════════════════════════════════════════════════╗
║                   COMPARISON TABLE                               ║
╚══════════════════════════════════════════════════════════════════╝

┌──────────────────┬─────────┬─────────┬──────────┬─────────────┐
│ Feature          │ Method 1│ Method 2│ Method 3 │ Best For    │
├──────────────────┼─────────┼─────────┼──────────┼─────────────┤
│ Stats only       │    ✓    │    ✗    │    ✗     │ Quick info  │
│ Full import      │    ✗    │    ✓    │    ✓     │ Complete    │
│ Template choice  │    ✗    │    ✓    │    ✗     │ Flexibility │
│ Browser opens    │    ✗    │    ✓    │    ✗     │ Convenience │
│ One-click        │    ✓    │    ✗    │    ✗     │ Speed       │
│ No auth needed   │    ✗    │    ✓*   │    ✓*    │ Security    │
│ Offline capable  │    ✗    │    ✗    │    ✓     │ Privacy     │
└──────────────────┴─────────┴─────────┴──────────┴─────────────┘

* Uses browser's existing session


╔══════════════════════════════════════════════════════════════════╗
║                    IMPLEMENTATION DETAILS                        ║
╚══════════════════════════════════════════════════════════════════╝

Method 2 Implementation:
  Function: downloadMylistExport()
  
  1. QInputDialog shows template list
  2. Selected template → build URL
  3. QDesktopServices opens browser
  4. URL: https://anidb.net/user/export
       ?template={selected}
       &list=mylist
  
  Templates available:
    - csv-adborg (default)
    - xml
    - csv
    - json
    - anidb

Method 3 Implementation:
  Function: importMylistFromFile()
  
  1. QFileDialog to select file
  2. Auto-detect format:
     - Extension check (xml, csv, txt)
     - Content check (XML tags)
     - Header detection (lid/aid columns)
  3. Parse using appropriate parser:
     - parseMylistXML() for XML
     - parseMylistCSV() for CSV/TXT
  4. Header-based parsing for CSV:
     - Builds column map
     - Extracts by column name
     - Supports any field order


╔══════════════════════════════════════════════════════════════════╗
║                         BENEFITS                                 ║
╚══════════════════════════════════════════════════════════════════╝

Complete MYLISTEXPORT Support:
  ✓ All template formats supported
  ✓ Intelligent format detection
  ✓ User-friendly template selection
  ✓ Browser integration
  ✓ No credentials needed
  ✓ Seamless workflow

Improved User Experience:
  ✓ Fewer steps to import
  ✓ Clear guidance
  ✓ Template recommendations
  ✓ Auto-import after download
  ✓ Error-tolerant parsing

Developer Benefits:
  ✓ Clean code separation
  ✓ Reusable parsers
  ✓ Extensible template support
  ✓ Well-documented
  ✓ Comprehensive testing data
```
