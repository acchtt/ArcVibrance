# ArcVibrance C1.10

## Saturation editor

- Replaced the WinUI `NumberBox` with a dedicated saturation editor made from a numeric `TextBox` and two inline repeat buttons.
- The arrow controls are permanently centered inside the field instead of opening in the compact spin-button flyout.
- Input accepts integers from 0 through 300 only.
- Typing, the slider, keyboard arrows, Page Up/Page Down, and press-and-hold spin buttons stay synchronized.
- Empty input is restored to the current saturation value when focus leaves the field.

## Settings wording

- Renamed startup, close behavior, appearance, and native-agent options with clearer user-facing language.
- Clarified that the background service can continue applying profiles after the interface closes.
