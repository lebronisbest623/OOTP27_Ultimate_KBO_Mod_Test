# csv-fast-reader

Vendored from https://github.com/jandoczy/csv-fast-reader at commit
`e9ff7ad8c3a9dc23bf964f53af9566f03800635f`.

License: MIT, see `LICENSE.txt`.

Local compatibility patch:
- include `<errno.h>` outside the Unix-only block so MinGW can see `EINVAL` and
  `ENOMEM`.
- use `UnmapViewOfFile` instead of `UnmapViewOfFileEx` for MinGW/Win32 API
  compatibility.
- remove an unused Windows local variable that trips `-Wall -Wextra`.
