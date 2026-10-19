Fonts of the AP text client (src/ap_ui.cpp), embedded into ap_client by apclient/embed_font.cmake. All SIL Open
Font License 1.1 (the *-OFL.txt files, copied to bin/licenses; no Reserved Font Names, so subsetting is allowed).
Merged in this order, the first font with a glyph wins:

NotoSans-Regular.ttf            Noto Sans Regular v2.015 (notofonts/latin-greek-cyrillic, unhinted): every glyph,
                                hinting and layout tables dropped. Latin, Greek, Cyrillic, Vietnamese, IPA, ...
NotoSansSymbols2-subset.ttf     Noto Sans Symbols 2 v2.008 (notofonts/symbols, unhinted): U+25A0-27BF (shapes,
                                misc symbols, dingbats: ★ ♥ ✓ ☀ ▲ ●)
NotoSansSymbols-subset.ttf      Noto Sans Symbols v2.003 (notofonts/symbols, unhinted): what Symbols 2 lacks in
                                U+2600-26FF (♀ ♂ ♪ ...)
NotoSansMath-subset.ttf         Noto Sans Math v3.000 (notofonts/math, unhinted): U+2190-22FF (arrows, math operators)

Made with fonttools pyftsubset, all with --layout-features='' --no-hinting:
  NotoSans-Regular.ttf --unicodes='*'
  NotoSansSymbols2-Regular.ttf --unicodes=U+25A0-27BF
  NotoSansSymbols-Regular.ttf --unicodes=<U+2600-26FF that NotoSansSymbols2 has no glyph for>
  NotoSansMath-Regular.ttf --unicodes=U+2190-22FF
