# Third-party licenses

This repository is licensed under the GNU GPLv3 as a whole (see
[LICENSE](LICENSE)). It vendors a small amount of third-party source under
`HPL2/dependencies/`, each under its own zlib-style license — all
GPL-compatible, so no part of this repository requires anything beyond
GPLv3 compliance to use or redistribute. Their licenses are reproduced below
for convenience; the authoritative copies ship alongside the vendored code.

## Newton Dynamics 3.14

`HPL2/dependencies/newton-dynamics/` — vendored from the actively maintained
[JulioJerez/newton-dynamics](https://github.com/JulioJerez/newton-dynamics)
repository (the physics engine this port's Newton bindings target). License
file: `HPL2/dependencies/newton-dynamics/LICENSE`.

```
Newton zlib license
Copyright (c) <2003-2011>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

Julio Jerez and Alain Suero
http://www.gzip.org/zlib/zlib_license.html
```

## AngelScript SDK addons

`HPL2/core/include/impl/{scriptarray.h,aswrappedcall.h}` and
`HPL2/core/{include,sources}/impl/{scriptarray.cpp,scripthelper.{h,cpp}}` are
vendored, lightly modified copies of the official AngelScript SDK addon
sources (`scriptarray`, `scripthelper`), matched to the AngelScript 2.38 API
this port builds against (the AngelScript *library* itself is a system
dependency — `angelscript-devel` — not vendored; only these addon-layer
source files, which AngelScript distributes as source for applications to
compile directly, are). Individual addon files don't carry their own
per-file header upstream either; the license is declared once for the whole
SDK:

```
AngelCode Scripting Library
Copyright (c) 2003-2023 Andreas Jönsson

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from
the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software in a
product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
```
(Source: `angelscript-devel`'s own packaged documentation,
`doc_license.html`.)

`HPL2/core/sources/impl/scriptstring.cpp` and `scriptstring_utils.cpp` are
Frictional Games' own custom string-handle addon (GPLv3, part of the main
codebase, not the AngelScript SDK's `scriptstdstring`).

## OALWrapper

`HPL2/dependencies/OALWrapper/` is Frictional Games' own C++ wrapper around
OpenAL, shipped by them under the same zlib-style terms as the two licenses
above. License file: `HPL2/dependencies/OALWrapper/LICENSE`.

## Game data is not included

None of the above changes what's actually in this repository: engine and
game *source code* only. Amnesia: The Dark Descent's maps, textures, audio,
and scripts are not open-source, are not included here, and are not covered
by any license in this repository — they remain Frictional Games' commercial
copyrighted content, and you need your own legitimate copy to use with a
binary built from this source.
