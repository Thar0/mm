# Soundfont XML Format Specification

Soundfont xmls always open with a Soundfont tag:
```xml
<Soundfont
    Name=""
    Index=""
    Medium=""
    CachePolicy=""
    SampleBank=""
    Indirect=""
    SampleBankDD=""
    IndirectDD=""
    LoopsHaveFrames=""
    PadToSize=""
>
```
- `Name`            : Soundfont symbol name. Must be a valid C identifier.
- `Index`           : Soundfont index. Must be an integer.
- `Medium`          : Storage medium. Must be an enum name from `SampleMedium`.
- `CachePolicy`     : Cache policy. Must be an enum name from `AudioCacheLoadType`.
- `SampleBank`      : Path to samplebank xml used by this soundfont.
- `Indirect`        : Pointer index if the samplebank is referenced indirectly.
- `SampleBankDD`    : Path to samplebank xml used for DD medium.
- `IndirectDD`      : Pointer index if the DD samplebank is referenced indirectly.
- `LoopsHaveFrames` : Whether loops in this soundfont store the total frame count of the sample. Must be a boolean.
- `PadToSize`       : For matching only. Specifies the total file size the result output should be padded to.


