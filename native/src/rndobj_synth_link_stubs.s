// AUTO-GENERATED weak no-op stubs for off-path symbols pulled in by the
// rndobj/ + synth/ matched-fork TUs (full object-graph load milestone).
// Categories: Wii GX render backend (RndMesh/RndTex draw, WiiRnd, TheRnd),
// Bink video, libogg/vorbis streaming, tomcrypt (mogg decrypt), Striper
// (mesh stripping), engine GPU hooks (GFX-off), game-layer globals
// (TheBandDirector). Each is WEAK -> any real definition wins; the rest
// resolve to a shared no-op (functions return 0; data reads yield the
// stub's address, a valid non-null pointer). None is on the .milo
// object-graph LOAD path. Regenerate: see native/README (nm -u | match).
    .text
    .p2align 4
__hmx_rndsynth_noop_stub:
    xorl %eax, %eax
    ret
    .weak BinkClose
    .set BinkClose, __hmx_rndsynth_noop_stub
    .weak BinkCloseTrack
    .set BinkCloseTrack, __hmx_rndsynth_noop_stub
    .weak BinkGetError
    .set BinkGetError, __hmx_rndsynth_noop_stub
    .weak BinkGetTrackData
    .set BinkGetTrackData, __hmx_rndsynth_noop_stub
    .weak BinkGoto
    .set BinkGoto, __hmx_rndsynth_noop_stub
    .weak BinkNextFrame
    .set BinkNextFrame, __hmx_rndsynth_noop_stub
    .weak BinkOpen
    .set BinkOpen, __hmx_rndsynth_noop_stub
    .weak BinkOpenTrack
    .set BinkOpenTrack, __hmx_rndsynth_noop_stub
    .weak BinkSetSoundTrack
    .set BinkSetSoundTrack, __hmx_rndsynth_noop_stub
    .weak BinkSetVideoOnOff
    .set BinkSetVideoOnOff, __hmx_rndsynth_noop_stub
    .weak ctr_decrypt
    .set ctr_decrypt, __hmx_rndsynth_noop_stub
    .weak ctr_reinit
    .set ctr_reinit, __hmx_rndsynth_noop_stub
    .weak ctr_start
    .set ctr_start, __hmx_rndsynth_noop_stub
    .weak DCZeroRange
    .set DCZeroRange, __hmx_rndsynth_noop_stub
    .weak gDebugFullQuota
    .set gDebugFullQuota, __hmx_rndsynth_noop_stub
    .weak Ntsc__6WiiRndFv
    .set Ntsc__6WiiRndFv, __hmx_rndsynth_noop_stub
    .weak ogg_page_serialno
    .set ogg_page_serialno, __hmx_rndsynth_noop_stub
    .weak ogg_stream_clear
    .set ogg_stream_clear, __hmx_rndsynth_noop_stub
    .weak ogg_stream_init
    .set ogg_stream_init, __hmx_rndsynth_noop_stub
    .weak ogg_stream_packetout
    .set ogg_stream_packetout, __hmx_rndsynth_noop_stub
    .weak ogg_stream_pagein
    .set ogg_stream_pagein, __hmx_rndsynth_noop_stub
    .weak ogg_stream_reset
    .set ogg_stream_reset, __hmx_rndsynth_noop_stub
    .weak ogg_sync_buffer
    .set ogg_sync_buffer, __hmx_rndsynth_noop_stub
    .weak ogg_sync_clear
    .set ogg_sync_clear, __hmx_rndsynth_noop_stub
    .weak ogg_sync_init
    .set ogg_sync_init, __hmx_rndsynth_noop_stub
    .weak ogg_sync_pageout
    .set ogg_sync_pageout, __hmx_rndsynth_noop_stub
    .weak ogg_sync_reset
    .set ogg_sync_reset, __hmx_rndsynth_noop_stub
    .weak ogg_sync_wrote
    .set ogg_sync_wrote, __hmx_rndsynth_noop_stub
    .weak register_cipher
    .set register_cipher, __hmx_rndsynth_noop_stub
    .weak rijndael_desc
    .set rijndael_desc, __hmx_rndsynth_noop_stub
    .weak rijndael_ecb_decrypt
    .set rijndael_ecb_decrypt, __hmx_rndsynth_noop_stub
    .weak rijndael_setup
    .set rijndael_setup, __hmx_rndsynth_noop_stub
    .weak TheBandDirector
    .set TheBandDirector, __hmx_rndsynth_noop_stub
    .weak TheRnd
    .set TheRnd, __hmx_rndsynth_noop_stub
    .weak TheUI
    .set TheUI, __hmx_rndsynth_noop_stub
    .weak TheWiiRnd
    .set TheWiiRnd, __hmx_rndsynth_noop_stub
    .weak vorbis_block_clear
    .set vorbis_block_clear, __hmx_rndsynth_noop_stub
    .weak vorbis_block_init
    .set vorbis_block_init, __hmx_rndsynth_noop_stub
    .weak vorbis_comment_clear
    .set vorbis_comment_clear, __hmx_rndsynth_noop_stub
    .weak vorbis_comment_init
    .set vorbis_comment_init, __hmx_rndsynth_noop_stub
    .weak vorbis_dsp_clear
    .set vorbis_dsp_clear, __hmx_rndsynth_noop_stub
    .weak vorbis_info_clear
    .set vorbis_info_clear, __hmx_rndsynth_noop_stub
    .weak vorbis_info_init
    .set vorbis_info_init, __hmx_rndsynth_noop_stub
    .weak vorbis_synthesis_blockin
    .set vorbis_synthesis_blockin, __hmx_rndsynth_noop_stub
    .weak vorbis_synthesis_headerin
    .set vorbis_synthesis_headerin, __hmx_rndsynth_noop_stub
    .weak vorbis_synthesis_init
    .set vorbis_synthesis_init, __hmx_rndsynth_noop_stub
    .weak vorbis_synthesis_pcmout
    .set vorbis_synthesis_pcmout, __hmx_rndsynth_noop_stub
    .weak vorbis_synthesis_poll
    .set vorbis_synthesis_poll, __hmx_rndsynth_noop_stub
    .weak vorbis_synthesis_read
    .set vorbis_synthesis_read, __hmx_rndsynth_noop_stub
    .weak vorbis_synthesis_restart
    .set vorbis_synthesis_restart, __hmx_rndsynth_noop_stub
    .weak _Z14CleanupGpuMeshP7RndMesh
    .set _Z14CleanupGpuMeshP7RndMesh, __hmx_rndsynth_noop_stub
    # _Z17CreateNativeSynthv (CreateNativeSynth) now strongly defined in
    # rb3_synth_native.cpp (headless base Synth) — stub removed.
    .weak _Z22DrawParticlesBillboardP14RndParticleSys
    .set _Z22DrawParticlesBillboardP14RndParticleSys, __hmx_rndsynth_noop_stub
    .weak _Z25HolmesClientCacheResourcePKcS0_
    .set _Z25HolmesClientCacheResourcePKcS0_, __hmx_rndsynth_noop_stub
    .weak _ZN11SynthSample7NewInstEbii
    .set _ZN11SynthSample7NewInstEbii, __hmx_rndsynth_noop_stub
    .weak _ZN12AsyncFileWii14FileExistsOnCDEPKc
    .set _ZN12AsyncFileWii14FileExistsOnCDEPKc, __hmx_rndsynth_noop_stub
    .weak _ZN12BandDirector12IsMusicVideoEv
    .set _ZN12BandDirector12IsMusicVideoEv, __hmx_rndsynth_noop_stub
    .weak _ZN12WaveFileDataC1ER8WaveFile
    .set _ZN12WaveFileDataC1ER8WaveFile, __hmx_rndsynth_noop_stub
    .weak _ZN12WaveFileDataD1Ev
    .set _ZN12WaveFileDataD1Ev, __hmx_rndsynth_noop_stub
    .weak _ZN13STRIPERRESULT19AllocLengthsAndRunsEii
    .set _ZN13STRIPERRESULT19AllocLengthsAndRunsEii, __hmx_rndsynth_noop_stub
    .weak _ZN13STRIPERRESULTaSERKS_
    .set _ZN13STRIPERRESULTaSERKS_, __hmx_rndsynth_noop_stub
    .weak _ZN13STRIPERRESULTC1ERKS_
    .set _ZN13STRIPERRESULTC1ERKS_, __hmx_rndsynth_noop_stub
    .weak _ZN13STRIPERRESULTC1Ev
    .set _ZN13STRIPERRESULTC1Ev, __hmx_rndsynth_noop_stub
    .weak _ZN13STRIPERRESULTD1Ev
    .set _ZN13STRIPERRESULTD1Ev, __hmx_rndsynth_noop_stub
    .weak _ZN14StandardStream23sReportLargeTimerErrorsE
    .set _ZN14StandardStream23sReportLargeTimerErrorsE, __hmx_rndsynth_noop_stub
    .weak _ZN6RndTex10SyncBitmapEv
    .set _ZN6RndTex10SyncBitmapEv, __hmx_rndsynth_noop_stub
    .weak _ZN6RndTex13PresyncBitmapEv
    .set _ZN6RndTex13PresyncBitmapEv, __hmx_rndsynth_noop_stub
    .weak _ZN6RndTex14MakeDrawTargetEv
    .set _ZN6RndTex14MakeDrawTargetEv, __hmx_rndsynth_noop_stub
    .weak _ZN6RndTex16FinishDrawTargetEv
    .set _ZN6RndTex16FinishDrawTargetEv, __hmx_rndsynth_noop_stub
    .weak _ZN6WiiRnd18GetProgressiveScanEv
    .set _ZN6WiiRnd18GetProgressiveScanEv, __hmx_rndsynth_noop_stub
    .weak _ZN7RndMesh11DrawShowingEv
    .set _ZN7RndMesh11DrawShowingEv, __hmx_rndsynth_noop_stub
    .weak _ZN7RndMesh11sRawCollideE
    .set _ZN7RndMesh11sRawCollideE, __hmx_rndsynth_noop_stub
    .weak _ZN7RndMesh12sLastCollideE
    .set _ZN7RndMesh12sLastCollideE, __hmx_rndsynth_noop_stub
    .weak _ZN7RndMesh18sUpdateApproxLightE
    .set _ZN7RndMesh18sUpdateApproxLightE, __hmx_rndsynth_noop_stub
    .weak _ZN7RndMesh6OnSyncEi
    .set _ZN7RndMesh6OnSyncEi, __hmx_rndsynth_noop_stub
    .weak _ZN7Striper4InitER13STRIPERCREATE
    .set _ZN7Striper4InitER13STRIPERCREATE, __hmx_rndsynth_noop_stub
    .weak _ZN7Striper7ComputeER13STRIPERRESULT
    .set _ZN7Striper7ComputeER13STRIPERRESULT, __hmx_rndsynth_noop_stub
    .weak _ZN8KeyChain10getNumKeysEv
    .set _ZN8KeyChain10getNumKeysEv, __hmx_rndsynth_noop_stub
    .weak _ZN8KeyChain9getMasherEPh
    .set _ZN8KeyChain9getMasherEPh, __hmx_rndsynth_noop_stub
    .weak _ZN8WaveFileC1ER9BinStream
    .set _ZN8WaveFileC1ER9BinStream, __hmx_rndsynth_noop_stub
    .weak _ZN8WaveFileD1Ev
    .set _ZN8WaveFileD1Ev, __hmx_rndsynth_noop_stub
    .weak _ZN9Transform6LookAtERK7Vector3S2_
    .set _ZN9Transform6LookAtERK7Vector3S2_, __hmx_rndsynth_noop_stub
    .weak _ZTV7BufFile
    .set _ZTV7BufFile, __hmx_rndsynth_noop_stub
