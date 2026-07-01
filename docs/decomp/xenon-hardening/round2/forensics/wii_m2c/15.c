typedef struct CampaignSongInfoPanel {
    /* 0x00 */ char pad0[0x38];
    /* 0x38 */ void **unk38;                        /* inferred */
} CampaignSongInfoPanel;                            /* size >= 0x3C */

? Unload__7UIPanelFv(UIPanel *this);                /* extern */
void Unload__21CampaignSongInfoPanelFv(CampaignSongInfoPanel *this); /* static */

/* CampaignSongInfoPanel::Unload (void) */
void Unload__21CampaignSongInfoPanelFv(CampaignSongInfoPanel *this) {
    void **temp_r3;

    Unload__7UIPanelFv((UIPanel *) this);
    temp_r3 = this->unk38;
    if (temp_r3 != NULL) {
        (*temp_r3)->unk8(1);
    }
    this->unk38 = NULL;
}