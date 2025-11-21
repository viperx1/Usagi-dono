# Anime Card UI Changes

## Download Button Addition

A new download button has been added to the anime card layout, positioned between the "Play Next" button and the "Reset Session" button.

### Visual Details:
- **Icon**: Down arrow (⬇)
- **Size**: Small stub button with maximum width of 30px
- **Style**: Font size 11pt, padding 4px 8px
- **Tooltip**: "Download next unwatched episode"
- **Position**: Immediately after "Play Next" button

### Layout:
```
[▶ Play Next] [⬇] [↻ Reset Session]
```

### Behavior:
- Emits `downloadAnimeRequested(int aid)` signal when clicked
- Hidden/shown along with other buttons when card is hidden/unhidden
- Currently a stub implementation - signal is emitted but no handler is connected yet

## File Version Selection (No Visual Changes)

The file version selection logic has been updated to prefer the newest file version (v2 over v1, v3 over v2, etc.) when:
1. Playing an episode from the card view
2. Using the "Play Next" button

This is a backend change with no direct visual impact, but users will notice that the highest version file is now automatically selected for playback.
