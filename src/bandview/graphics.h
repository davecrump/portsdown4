#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

void waterfall_render_fft(uint8_t *fft_data);

void graphics_frequency_newdata(void);
void graphics_if_fft_newdata(uint8_t *fft_data);

void ptt_button_generate(void);
void ptt_button_render(void);

#endif /* __GRAPHICS_H__ */