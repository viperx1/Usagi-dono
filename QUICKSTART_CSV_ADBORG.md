# Quick Start: Using csv-adborg Template

## For End Users

### Step 1: Export from AniDB with csv-adborg Template

1. Login to AniDB at https://anidb.net
2. Go to your MyList: https://anidb.net/perl-bin/animedb.pl?show=mylist
3. Click **"Export"** at the top
4. In the template dropdown, select **"csv-adborg"**
5. Click **"Export"** button
6. Save the downloaded CSV file to your computer

### Step 2: Import into Usagi-dono

1. Open **Usagi-dono** application
2. Navigate to the **"MyList"** tab
3. Click **"Import MyList from File"** button
4. Select the csv-adborg file you downloaded
5. Wait for the import to complete
6. Success message will show: "Successfully imported N mylist entries"

### Step 3: View Your MyList

1. Click **"Load MyList from Database"**
2. Your imported anime and episodes will display in the tree view
3. All data is now available for tracking and management

## What's Different in csv-adborg?

The csv-adborg template has a different field order than the standard export:

| Field | csv-adborg Position | Standard Position |
|-------|-------------------|-------------------|
| aid (Anime ID) | 1st | 4th |
| eid (Episode ID) | 2nd | 3rd |
| gid (Group ID) | 3rd | 5th |
| lid (MyList ID) | 4th | 1st |

**Key Benefit**: The application now automatically detects which format you're using, so you don't need to worry about it!

## Troubleshooting

### Import shows 0 entries
- Make sure your file has a header row
- Check that the file contains at least "lid" and "aid" columns
- Look at the log output for specific error messages

### Some entries are missing
- Entries without lid or aid will be skipped
- Check the import count in the success message
- Compare with the number of lines in your CSV file

### fid shows as 0 for all entries
- This is expected for csv-adborg format (it doesn't include fid)
- This won't affect MyList display or most features
- File matching functionality may be limited

## Supported Templates

The following AniDB MYLISTEXPORT templates are confirmed to work:
- ✅ csv-adborg
- ✅ Standard CSV export (default)
- ✅ Any custom template with lid and aid columns

## Need Help?

Check the documentation:
- `MYLIST_IMPORT_GUIDE.md` - Complete user guide
- `CSV_ADBORG_SUPPORT.md` - Technical details
- `IMPLEMENTATION_CSV_ADBORG.md` - Implementation summary

## Example Files

Test data examples are available in `tests/data/`:
- `test_csv_adborg.csv` - Sample csv-adborg format
- `test_standard_csv.csv` - Sample standard format
- `test_mixed_case.csv` - Case-insensitive example
- `test_minimal_fields.csv` - Minimal fields example
