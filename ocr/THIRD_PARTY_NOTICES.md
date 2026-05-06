# Bundled OCR Runtime Notices

This directory bundles a minimal Windows runtime for Tesseract OCR so the launcher can run player-profile menu OCR without requiring users to install Tesseract system-wide.

## Tesseract OCR

- Component: `tesseract.exe`, `libtesseract-5.dll`
- Version: `5.4.0.20240606`
- Source/build distribution: UB Mannheim Tesseract Windows build
- Upstream project: https://github.com/tesseract-ocr/tesseract
- License: Apache License 2.0
- License text: `LICENSE.Tesseract.txt`

## Tesseract English Trained Data

- Component: `tessdata/eng.traineddata`
- Component: `tessdata/configs/tsv`
- Upstream project: https://github.com/tesseract-ocr/tessdata
- License: Apache License 2.0
- License text: `tessdata/LICENSE.tessdata.txt`

## Runtime DLLs

This bundle includes DLLs required by the UB Mannheim Windows build of Tesseract. The launcher uses Tesseract only as a local command-line executable for OCR over a temporary image crop.

Included runtime files:

- `libarchive-13.dll`
- `libb2-1.dll`
- `libbz2-1.dll`
- `libcrypto-3-x64.dll`
- `libdeflate.dll`
- `libexpat-1.dll`
- `libgcc_s_seh-1.dll`
- `libgif-7.dll`
- `libiconv-2.dll`
- `libjbig-0.dll`
- `libjpeg-8.dll`
- `libleptonica-6.dll`
- `libLerc.dll`
- `liblz4.dll`
- `liblzma-5.dll`
- `libopenjp2-7.dll`
- `libpng16-16.dll`
- `libsharpyuv-0.dll`
- `libstdc++-6.dll`
- `libtiff-6.dll`
- `libwebp-7.dll`
- `libwebpmux-3.dll`
- `libwinpthread-1.dll`
- `libzstd.dll`
- `zlib1.dll`

Leptonica, used by Tesseract for image handling, is distributed under a permissive BSD-style license. Other bundled runtime libraries are standard open-source runtime dependencies from the Windows build.

Before publishing a public release, refresh this file if the bundled Tesseract build changes.
