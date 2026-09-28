/* Stands in for a bundled third-party C library (miniaudio, ...): default visibility
 * unless the policy hides it. */
int ma_fake_init(void) { return 1; }
int ma_fake_uninit(void) { return 0; }
