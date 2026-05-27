#ifdef HX_NATIVE

// vorbis_synthesis_poll was Harmonix's incremental decoder for Xbox.
// On native/web, delegate to standard vorbis_synthesis which does full decode at once.
// Must be extern "C": VorbisReader.h includes oggvorbis/codec.h which declares it
// inside an `extern "C"` block, so the call site expects C linkage.
struct vorbis_block;
struct ogg_packet;
extern "C" {
    int vorbis_synthesis(vorbis_block *vb, ogg_packet *op);
    int vorbis_synthesis_poll(vorbis_block *vb, ogg_packet *op) {
        return vorbis_synthesis(vb, op);
    }
}

#endif // HX_NATIVE
