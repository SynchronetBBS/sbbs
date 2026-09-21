/* help_card.h -- the door's key-help card (F1 / Ctrl-K), drawn as plain
 * positioned ANSI over the frozen picture, like SyncSCUMM's. */
#ifndef SYNCDRIVE_HELP_CARD_H_
#define SYNCDRIVE_HELP_CARD_H_

/* Draw the card through termgfx_termio_write(). The caller stops the game and
 * frame presentation while it is up. */
void help_card_show(void);

/* Repaint the game over it (the next present sends a whole frame). */
void help_card_dismiss(void);

#endif
