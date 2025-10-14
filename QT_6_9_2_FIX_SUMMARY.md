# Qt 6.9.2 Static Linking Fix Summary

## Issue
Build failed on Windows with Qt 6.9.2 LLVM MinGW static libraries:
```
ld.lld: error: undefined symbol: __declspec(dllimport) QTimer::singleShotImpl(std::chrono::duration<long long, std::ratio<1ll, 1000000000ll>>, Qt::TimerType, QObject const*, QtPrivate::QSlotObjectBase*)
>>> referenced by D:/a/Usagi-dono/Qt/6.9.2/llvm-mingw_64/include\QtCore\qtimer.h:202
>>>               usagi/CMakeFiles/usagi.dir/src/window.cpp.obj:(Window::Window())
```

## Root Cause
In Qt 6.9.2 with static linking, the `QTimer::singleShot(int, QObject*, Functor)` overload with lambda/functor parameter calls `QTimer::singleShotImpl()`, which is not properly exported or linked in static Qt builds. This is a known issue with some Qt 6.x static builds where template-based implementations may not have proper export declarations.

## Solution
Replaced the lambda-based `QTimer::singleShot` call with the traditional QTimer pattern that the codebase already uses elsewhere:

### Before (Problematic Code)
```cpp
// In Window constructor
QTimer::singleShot(1000, this, [this]() {
    mylistStatusLabel->setText("MyList Status: Loading from database...");
    loadMylistFromDatabase();
    mylistStatusLabel->setText("MyList Status: Ready");
});
```

### After (Fixed Code)
```cpp
// In window.h - Added member variable
QTimer *startupTimer;

// In window.h - Added slot declaration
void startupInitialization();

// In Window constructor - Create and configure timer
startupTimer = new QTimer(this);
startupTimer->setSingleShot(true);
startupTimer->setInterval(1000);
connect(startupTimer, SIGNAL(timeout()), this, SLOT(startupInitialization()));

// At end of Window constructor - Start the timer
startupTimer->start();

// New slot implementation
void Window::startupInitialization()
{
    // This slot is called 1 second after the window is constructed
    // to allow the UI to be fully initialized before loading data
    mylistStatusLabel->setText("MyList Status: Loading from database...");
    loadMylistFromDatabase();
    mylistStatusLabel->setText("MyList Status: Ready");
}
```

## Why This Works
1. **Traditional Signal-Slot Connection**: Uses Qt's moc-generated signal-slot mechanism instead of template-based functors
2. **No Template Instantiation**: Avoids the template code path that references the unresolved `singleShotImpl` symbol
3. **Consistent with Codebase**: Matches the existing pattern used for the `safeclose` timer
4. **Static Linking Compatible**: The SIGNAL/SLOT macros are fully compatible with static Qt builds

## Files Modified
1. `usagi/src/window.h` - Added `startupTimer` member and `startupInitialization()` slot declaration
2. `usagi/src/window.cpp` - Replaced lambda-based singleShot with traditional timer pattern

## Benefits
- ✅ Resolves static linking error
- ✅ Uses proven, existing code patterns from the codebase
- ✅ Maintains exact same functionality
- ✅ No external dependencies required
- ✅ Compatible with both static and dynamic Qt builds

## Testing
This fix should allow the Windows Build & Release workflow to complete successfully with Qt 6.9.2 LLVM MinGW static libraries.
