# Third-party notices

Thermavip is distributed under the BSD 3-Clause licence recorded in `LICENSE`.
It also incorporates the third-party code listed below, which keeps its own
licence. Redistributing Thermavip means redistributing these notices with it.

## LdrDllNotificationHook

| | |
|---|---|
| Upstream | https://github.com/m417z/LdrDllNotificationHook |
| Licence | MIT |
| Copyright | Copyright 2023 Michael Maltsev |
| Files | `src/Logging/LdrDllNotificationHook.h`, `src/Logging/LdrDllNotificationHook.cpp`, `src/Logging/LdrInit.cpp` |
| Revision taken | not recorded |

These three units are compiled into `VipLogging`, so the notice travels with
every binary distribution.

The upstream revision is unknown: the copy carries no version marker and none
was recorded when it was taken. Until it is identified there is no way to tell
whether the local copy has diverged, or whether an upstream fix applies to it.

### MIT licence text

```
Copyright 2023 Michael Maltsev

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
