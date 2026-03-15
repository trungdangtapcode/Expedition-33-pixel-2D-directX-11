# Dynamic Data-Driven Command Menu Rendering

The Command Menu phase (`PlayerInputPhase::COMMAND_SELECT`) inside Turn-Based battle uses a data-driven approach to populate choices (such as Fight, Flee, Items) without hardcoding UI logic to specific command types.

## Strategy: No Hardcoding

Rather than manually invoking render functions for `Fight` and `Flee` buttons, we utilize our Input Controller's command list directly:
```cpp
const auto& commands = mInputController.GetCommands();
```
We rely on the `ICommand` abstract interface which exposes `GetLabel()` to retrieve whatever the command is named in the system:
```cpp
const char* GetLabel() const = 0;
```
This guarantees UI components strictly observe data, decoupling rendering logic from gameplay flow.

## Screen-Space Anchoring

Unlike the Skill Selection dialog, which hovers next to the corresponding player character in World Space (thus moving when the camera pans), the main Command List anchors to the screen UI (Screen Space). 

Instead of specifying absolute global pixel values (like `y = 800`), we anchor logically from the bottom-left corner of the window. We fetch live screen dimensions from DirectX context and mathematically compute constraints:

```cpp
const float screenW = static_cast<float>(mD3D.GetWidth());
const float screenH = static_cast<float>(mD3D.GetHeight());

const float paddingLeft = 40.0f;
const float paddingBottom = 40.0f;
```

This stack iterates downwards (or upwards, depending on the math direction) cleanly packing each instantiated `mDialogBox` without overlaps:

```cpp
const float totalHeight = commandCount * (baseDialogHeight + 10.0f);
const float startY = screenH - paddingBottom - totalHeight;
```

## Scaling the Nine-Slice Dialogs

To signal which element is currently hovered by the user (tracked securely by `mInputController.GetCommandIndex()`), we proportionally scale the visual width and heights:

```cpp
bool isHovered = (i == hoveredIndex);
float scaleMultiplier = isHovered ? 1.05f : 1.0f;
```

Because `mDialogBox.Draw()` renders from the top-left coordinate, simply passing a greater width would cause the right edge to extrude unevenly. We preserve volumetric centering by shifting `dialogX` and `dialogY` backwards precisely by half the delta difference.

```cpp
float offsetX = (dialogWidth - baseDialogWidth) / 2.0f;
float offsetY = (dialogHeight - baseDialogHeight) / 2.0f;

float dialogX = startX - offsetX;
float dialogY = startY + (i * (baseDialogHeight + 10.0f)) - offsetY;
```

Finally, we project strictly utilizing Screen-Space Identity Matrix to ignore `BattleCameraController` zoom algorithms:

```cpp
auto identityMatrix = DirectX::XMMatrixIdentity();

mDialogBox.Draw(..., identityMatrix);
mTextRenderer.DrawString(..., identityMatrix);
```