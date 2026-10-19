NotoSans-Regular-subset.ttf: Noto Sans Regular v2.015 (unhinted, https://github.com/notofonts/latin-greek-cyrillic/
releases/tag/NotoSans-v2.015), subset to Latin, Latin-1, Latin Extended-A/B, Greek, Cyrillic, general punctuation,
euro, trademark and the replacement character. SIL Open Font License 1.1 (NotoSans-OFL.txt; no Reserved Font Name).
Made with fonttools:
  pyftsubset NotoSans-Regular.ttf --unicodes="U+0020-007E,U+00A0-024F,U+0370-03FF,U+0400-04FF,U+2000-206F,U+20AC,U+2122,U+FFFD" \
    --layout-features='' --no-hinting --output-file=NotoSans-Regular-subset.ttf
Embedded into ap_client by apclient/embed_font.cmake (the ImGui text client's font).
