#!/usr/bin/env python3
# audio_extract.py
#
#   Extract audio files
#

import argparse, os, shutil, time
from multiprocessing.pool import ThreadPool
from typing import List, Union

from audio_tables import AudioCodeTable, AudioStorageMedium
from audiobank_structs import AudioSampleCodec
from audiotable import AudioTableData, AudioTableFile, AudioTableSample
from audiobank_file import AudiobankFile
from disassemble_sequence import CMD_SPEC, SequenceDisassembler

from util import align, debugm, error, incbin, program_get, XMLWriter

from config import GAMEVERSION_ALL_OOT, VERSION_TABLE, GameVersionInfo
from config import Z64SAMPLE_PATH
from config import AUDIOTABLE_BUFFER_BUGS, FAKE_BANKS
from config import SEQ_DISAS_HACKS
from config import HANDWRITTEN_SEQUENCES_OOT, HANDWRITTEN_SEQUENCES_MM

# ======================================================================================================================
#   Run
# ======================================================================================================================

def collect_sample_banks(rom_image : memoryview, version_info : GameVersionInfo, table : AudioCodeTable):
    sample_banks = []

    for i,entry in enumerate(table):
        entry : AudioCodeTable.AudioCodeTableEntry

        assert entry.short_data1 == 0 and entry.short_data2 == 0 and entry.short_data3 == 0, \
               "Bad data for Sample Bank entry, all short data should be 0"
        assert entry.medium == AudioStorageMedium.MEDIUM_CART , \
               "Bad data for Sample Bank entry, medium should be CART"

        if entry.size == 0:
            # Pointer to other entry, in this case the rom address is a table index

            entry_dst = table.entries[entry.rom_addr]
            sample_banks[entry.rom_addr].register_ptr(i)
            sample_banks.append(entry_dst.rom_addr)

            # debugm(f"{i} Pointer: {entry.rom_addr} -> 0x{entry_dst.rom_addr:X}, 0x{entry_dst.size:X}")
        else:
            # Check whether this samplebank suffers from the buffer bug
            if version_info.version_id in AUDIOTABLE_BUFFER_BUGS:
                bug = i in AUDIOTABLE_BUFFER_BUGS[version_info.version_id]
            else:
                bug = False

            bank = AudioTableFile(i, rom_image, entry, version_info.audiotable_rom + table.rom_addr, buffer_bug=bug)
            os.makedirs(f"baserom/audiotable_files", exist_ok=True)
            bank.dump_bin(f"baserom/audiotable_files/Samplebank_{i}.bin")

            sample_banks.append(bank)

    return sample_banks

def bank_data_lookup(sample_banks : List[AudioTableFile], e : Union[AudioTableFile, int]) -> AudioTableFile:
    if isinstance(e, int):
        if e == 255:
            return None
        return bank_data_lookup(sample_banks, sample_banks[e])
    else:
        return e

def collect_soundfonts(rom_image : memoryview, sound_font_table : AudioCodeTable, sample_banks : List[AudioTableFile],
                       version_info : GameVersionInfo):
    soundfonts = []

    for i,entry in enumerate(sound_font_table):
        entry : AudioCodeTable.AudioCodeTableEntry

        # Lookup the samplebanks used by this soundfont
        bank1 = bank_data_lookup(sample_banks, FAKE_BANKS[version_info.version_id].get(i, entry.sample_bank_id_1))
        bank2 = bank_data_lookup(sample_banks, entry.sample_bank_id_2)

        # debugm(f"\n\nBANK BEGIN {i} {e.sample_bank_id_1} {e.sample_bank_id_2}\n\n\n")

        # Read the data
        soundfont = AudiobankFile(rom_image, i, entry, version_info.audiobank_rom + sound_font_table.rom_addr, bank1,
                                  bank2, entry.sample_bank_id_1, entry.sample_bank_id_2)
        soundfonts.append(soundfont)

        # Write the individual file for debugging and comparison
        os.makedirs(f"baserom/audiobank_files", exist_ok=True)
        soundfont.dump_bin(f"baserom/audiobank_files/{soundfont.file_name}.bin")

    return soundfonts

def aifc_extract_one_sample(base_path : str, sample : AudioTableSample, n : int):
    # export to AIFC
    sample.to_file(f"{base_path}/aifc/Sample{n}.aifc")
    # decode to AIFF/WAV
    half = ".half" if sample.header.codec == AudioSampleCodec.CODEC_SMALL_ADPCM else ""
    out = program_get(f"{Z64SAMPLE_PATH} --matching {base_path}/aifc/Sample{n}.aifc {base_path}/Sample{n}{half}.wav")
    # TODO remove the AIFC? (after testing)

def aifc_extract_one_bin(base_path : str, sample : AudioTableData):
    # export to BIN
    filename = f"UNACCOUNTED_{sample.start:X}_{sample.end:X}.bin"
    sample.to_file(f"{base_path}/aifc/{filename}")
    # copy to correct location
    shutil.copyfile(f"{base_path}/aifc/{filename}", f"{base_path}/{filename}")
    # TODO move instead of copy? (after testing)

def extract_samplebank(pool : ThreadPool, sample_banks : List[AudioTableFile], bank : AudioTableFile, i : int,
                       write_xml : bool):
    debugm(f"SAMPLE BANK {i}")

    # deal with remaining gaps, have to blob them unless we can find an exact match in another bank
    bank.finalize_coverage(sample_banks)

    base_path = f"assets/samples/Bank{i}"

    # write xml
    os.makedirs(f"assets/samplebanks", exist_ok=True)
    with open(f"assets/samplebanks/Samplebank_{i}.xml", "w") as outfile:
        outfile.write(bank.to_xml(base_path))

    # write the extraction xml if specified
    if write_xml:
        os.makedirs(f"assets/xml/samplebanks", exist_ok=True)
        bank.write_extraction_xml(f"assets/xml/samplebanks/Samplebank_{i}.xml")

    os.makedirs(f"{base_path}/aifc", exist_ok=True)

    aifc_samples = [sample for sample in bank.samples_final if     isinstance(sample, AudioTableSample)]
    bin_samples  = [sample for sample in bank.samples_final if not isinstance(sample, AudioTableSample)]

    t_start = time.time()

    # we assume the number of bin samples are very small and don't multiprocess it
    for j,sample in enumerate(bin_samples):
        aifc_extract_one_bin(base_path, sample)

    # multiprocess aifc extraction + decompression
    async_results = [pool.apply_async(aifc_extract_one_sample, args=(base_path, sample, j)) for j,sample in enumerate(aifc_samples)]
    # block until done
    [res.get() for res in async_results]

    debugm(f"Took {time.time() - t_start}s")

def extract_sequences(sequence_table : AudioCodeTable, sequence_font_table : memoryview, soundfonts : List[AudiobankFile],
                      rom_image : memoryview, version_info : GameVersionInfo, write_xml : bool):

    sequence_font_table_cvg = [0] * len(sequence_font_table)

    # Select list of handwritten sequences
    if version_info.version_id in GAMEVERSION_ALL_OOT:
        handwritten_sequences = HANDWRITTEN_SEQUENCES_OOT
    else:
        handwritten_sequences = HANDWRITTEN_SEQUENCES_MM

    all_fonts = []

    seq_enum_names = []
    sequence_table_hdr = None
    with open(f"include/tables/sequence_table.h", "r") as infile:
        sequence_table_hdr = infile.read()

    for line in sequence_table_hdr.split("\n"):
        line = line.strip()
        if line.startswith("DEFINE_SEQUENCE"):
            seq_enum_names.append(line.split(",")[1].strip())

    assert len(seq_enum_names) == len(sequence_table)

    if write_xml:
        os.makedirs(f"assets/xml/sequences", exist_ok=True)

    for i,entry in enumerate(sequence_table):
        entry : AudioCodeTable.AudioCodeTableEntry

        # extract font indices
        font_data_offset = (sequence_font_table[2 * i + 0] << 8) | (sequence_font_table[2 * i + 1])
        num_fonts = sequence_font_table[font_data_offset]
        font_data_offset += 1
        fonts = sequence_font_table[font_data_offset:font_data_offset+num_fonts]

        all_fonts.append(fonts)

        # mark coverage for sequence font table
        sequence_font_table_cvg[2 * i + 0] = 1
        sequence_font_table_cvg[2 * i + 1] = 1
        for j in range(font_data_offset-1,font_data_offset+num_fonts):
            sequence_font_table_cvg[j] = 1

        if entry.size != 0:
            # Real sequence, extract

            seq_data = bytearray(entry.data(rom_image, version_info.audioseq_rom))

            ext = ".prg" if i in handwritten_sequences else ""

            # (Debug) extract original sequence binary for comparison
            os.makedirs(f"baserom/audioseq_files", exist_ok=True)
            with open(f"baserom/audioseq_files/seq_{i}{ext}.aseq", "wb") as outfile:
                outfile.write(seq_data)

            # TODO from xml
            sequence_name = f"Sequence_{i}"

            # Write extraction xml entry
            if write_xml:
                xml = XMLWriter()

                xml.write_element("Sequence", {
                    "Name" : sequence_name,
                    "Index" : i,
                })

                with open(f"assets/xml/sequences/Sequence_{i}.xml", "w") as outfile:
                    outfile.write(str(xml))

            if i in handwritten_sequences:
                # skip writing out "handwritten" sequences
                continue

            print(f"Extract {sequence_name}")

            #debugm("=======================================================")
            #debugm(f"SEQUENCE {i} [size=0x{entry.size:X}] [fonts={[f'0x{b:02X}' for b in fonts]}]")
            #debugm("=======================================================")
            #debugm(entry)

            # TODO also pass the relevant soundfont(s) files for proper instrument enum names
            disas = SequenceDisassembler(i, seq_data, SEQ_DISAS_HACKS[version_info.version_id].get(i, None), CMD_SPEC,
                                         version_info.mml_version, f"assets/sequences/seq_{i}.seq",
                                         sequence_name, [soundfonts[i] for i in fonts], seq_enum_names)
            disas.analyze()
            disas.emit()
        else:
            # Pointer to another sequence
            #debugm("POINTER")
            #debugm(entry)
            # TODO handle this (oot seq 87, mm several)
            pass

    # Check full coverage
    try:
        if align(sequence_font_table_cvg.index(0), 16) != len(sequence_font_table_cvg):
            # does not pad to full size, fail
            assert False, "Sequence font table missing data"
        # pads to full size, good
    except ValueError:
        pass # fully covered, good

    # Check consistency of font data for the same sequence accessed via pointers
    # debugm("FONT CHECK")

    for i,entry in enumerate(sequence_table):
        entry : AudioCodeTable.AudioCodeTableEntry

        # Fonts for this entry
        fonts = all_fonts[i]

        if entry.size != 0:
            # real, ignore
            pass
        else:
            # pointer, check that the fonts for this entry are the same as the fonts for the other
            j = entry.rom_addr

            fonts2 = all_fonts[j]

            debugm(f"{i} -> {j}")
            debugm(list(bytes(fonts)))
            debugm(list(bytes(fonts2)))

            assert fonts == fonts2, \
                   f"Font mismatch: Pointer {i} against Real {j}. This is a limitation of the build process."

def extract_with_full_analysis(version_name : str, rom_path : str, write_xml : bool):
    # Get version info

    if version_name not in VERSION_TABLE:
        error(f"Invalid version: {version_name}, must be one of {set(VERSION_TABLE.keys())}")

    version_info = VERSION_TABLE[version_name]

    # Open baserom (uncompressed)

    with open(rom_path, "rb") as infile:
        baserom = bytearray(infile.read())
    rom_image = memoryview(baserom)

    # ==================================================================================================================
    # Collect audio tables
    # ==================================================================================================================

    seq_font_tbl_len = version_info.seq_table - version_info.seq_font_table

    sound_font_table = AudioCodeTable(rom_image, version_info.soundfont_table)
    sample_bank_table = AudioCodeTable(rom_image, version_info.sample_bank_table)
    sequence_table = AudioCodeTable(rom_image, version_info.seq_table)
    sequence_font_table = incbin(rom_image, version_info.seq_font_table, seq_font_tbl_len)

    # (Debug) Extract Table Binaries

    os.makedirs(f"baserom/audio_code_tables/", exist_ok=True)

    with open(f"baserom/audio_code_tables/samplebank_table.bin", "wb") as outfile:
        outfile.write(sample_bank_table.data)

    with open(f"baserom/audio_code_tables/soundfont_table.bin", "wb") as outfile:
        outfile.write(sound_font_table.data)

    with open(f"baserom/audio_code_tables/sequence_table.bin", "wb") as outfile:
        outfile.write(sequence_table.data)

    with open(f"baserom/audio_code_tables/sequence_font_table.bin", "wb") as outfile:
        outfile.write(sequence_font_table)

    # ==================================================================================================================
    # Collect samplebanks
    # ==================================================================================================================

    sample_banks = collect_sample_banks(rom_image, version_info, sample_bank_table)

    # ==================================================================================================================
    # Collect soundfonts
    # ==================================================================================================================
    #   Soundfonts
    #

    soundfonts = collect_soundfonts(rom_image, sound_font_table, sample_banks, version_info)

    # ==================================================================================================================
    # Finalize samplebanks
    # ==================================================================================================================

    for i,bank in enumerate(sample_banks):
        if isinstance(bank, AudioTableFile):
            bank.finalize_samples()

    # TODO By this point we must have fully decided upon a samplerate + basenote to put in the sample

    # ==================================================================================================================
    # Extract samplebank contents
    # ==================================================================================================================

    # Check that the z64sample binary is available
    assert os.path.isfile(Z64SAMPLE_PATH) , "Compile z64sample!!"

    with ThreadPool(processes=os.cpu_count()) as pool:
        for i,bank in enumerate(sample_banks):
            if isinstance(bank, AudioTableFile):
                extract_samplebank(pool, sample_banks, bank, i, write_xml)

    # ==================================================================================================================
    # Extract soundfonts
    # ==================================================================================================================

    for i,sf in enumerate(soundfonts):
        sf : AudiobankFile

        # Finalize instruments/drums/etc.
        # This step includes assigning the final samplerate and basenote for the instruments, which may be different
        # from the samplerate and basenote assigned to their sample prior.
        sf.finalize()

        # write the soundfont xml itself
        os.makedirs(f"assets/soundfonts", exist_ok=True)
        with open(f"assets/soundfonts/{sf.file_name}.xml", "w") as outfile:
            outfile.write(sf.to_xml(f"Soundfont_{i}", f"assets/samplebanks"))

        # write the extraction xml if specified
        if write_xml:
            os.makedirs(f"assets/xml/soundfonts", exist_ok=True)
            sf.write_extraction_xml(f"assets/xml/soundfonts/{sf.file_name}.xml")

    # ==================================================================================================================
    # Extract sequences
    # ==================================================================================================================

    extract_sequences(sequence_table, sequence_font_table, soundfonts, rom_image, version_info, write_xml)

def extract_with_xmls(version_name : str, rom_path : str, xmls_path : str):
    # TODO implement this
    assert False, "Not Yet Implemented!"

if __name__ == '__main__':
    # TODO 2 modes:
    # default : extract only what is needed to build using pre-generated xmls
    # --full  : extract everything including things that are ordinarily committed
    parser = argparse.ArgumentParser(description="baserom asset extractor")
    parser.add_argument("-r", "--rom", required=True, help="path to baserom image")
    parser.add_argument("-v", "--version", required=True, help="baserom version")
    parser.add_argument("--full", required=False, action="store_true", help="Run full analysis")
    parser.add_argument("--write-xml", required=False, action="store_true", help="Write xml files")
    args = parser.parse_args()

    if args.full:
        extract_with_full_analysis(args.version, args.rom, args.write_xml)
    else:
        extract_with_xmls(args.version, args.rom, "assets/xml")
