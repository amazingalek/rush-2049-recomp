# Jet Force Gemini (N64)

Status: **triage done, research in progress.** Second target, after Rush 2049.

## The ROM we build against

| Field | Value |
|---|---|
| Dump | `Jet Force Gemini (U) [!].z64` (GoodN64-verified), in Alek's ROM set on `D:\Torrents\…\Nintendo.N64.ROMs.Full.Set.US` |
| Size | 32 MiB |
| SHA-1 | `493ced9008dbe932d6e91179b68e8630cf23a023` |
| MD5 | `772cc6eab2620d2d3cdc17bbc26c4f68` |
| Header name / ID | `JET FORCE GEMINI` / `NJFE`, revision 0 |
| CRC1/CRC2 | `8a6009b6` / `94ace150` |
| Entrypoint | `0x80000400` |
| libultra release field | `0x1447` (`G`: libultra 2.0G) |

## Triage (`py -3 -I tools/rom_triage.py <rom>`)

- **No plaintext ucode string.** The graphics ucode is inside compressed data, so it can't be
  checked against RT64's table until the ROM is decompressed.
- **Compressed.** One gzip header (`1f 8b 08`). Rare titles of this era use Rare's
  deflate-based compression, which BanjoRecomp already handles for Banjo-Kazooie
  (`rom_decompression.cpp`, `patches/overlay_loading.c`).
- Strings `ucode_boot`, `ucode_boot_size`, `ucode_data`, `ucode_data_size` are in plain text.
- Rare credits strings ("TM and < 1999 rare", "Rare USA") are present.
