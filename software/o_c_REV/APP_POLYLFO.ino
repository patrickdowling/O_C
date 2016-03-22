#include "OC_apps.h"
#include "OC_digital_inputs.h"
#include "OC_menus.h"

#include "util/util_math.h"
#include "util/util_settings.h"
#include "frames_poly_lfo.h"

enum POLYLFO_SETTINGS {
  POLYLFO_SETTING_COARSE,
  POLYLFO_SETTING_FINE,
  POLYLFO_SETTING_SHAPE,
  POLYLFO_SETTING_SHAPE_SPREAD,
  POLYLFO_SETTING_SPREAD,
  POLYLFO_SETTING_COUPLING,
  POLYLFO_SETTING_FREQ_DIV_B,
  POLYLFO_SETTING_FREQ_DIV_C,
  POLYLFO_SETTING_FREQ_DIV_D,
  POLYLFO_SETTING_A_XOR_B,
  POLYLFO_SETTING_B_XOR_C,
  POLYLFO_SETTING_C_XOR_D,
  POLYLFO_SETTING_LAST
};



class PolyLfo : public settings::SettingsBase<PolyLfo, POLYLFO_SETTING_LAST> {
public:

  uint16_t get_coarse() const {
    return values_[POLYLFO_SETTING_COARSE];
  }

  int16_t get_fine() const {
    return values_[POLYLFO_SETTING_FINE];
  }

  uint16_t get_shape() const {
    return values_[POLYLFO_SETTING_SHAPE];
  }

  uint16_t get_shape_spread() const {
    return values_[POLYLFO_SETTING_SHAPE_SPREAD];
  }

  uint16_t get_spread() const {
    return values_[POLYLFO_SETTING_SPREAD];
  }

  uint16_t get_coupling() const {
    return values_[POLYLFO_SETTING_COUPLING];
  }

  frames::PolyLfoFreqDivisions get_freq_div_b() const {
    return static_cast<frames::PolyLfoFreqDivisions>(values_[POLYLFO_SETTING_FREQ_DIV_B]);
  }

  frames::PolyLfoFreqDivisions get_freq_div_c() const {
    return static_cast<frames::PolyLfoFreqDivisions>(values_[POLYLFO_SETTING_FREQ_DIV_C]);
  }

  frames::PolyLfoFreqDivisions get_freq_div_d() const {
    return static_cast<frames::PolyLfoFreqDivisions>(values_[POLYLFO_SETTING_FREQ_DIV_D]);
  }

  bool get_a_xor_b() const {
    return values_[POLYLFO_SETTING_A_XOR_B];
  }

  bool get_b_xor_c() const {
    return values_[POLYLFO_SETTING_B_XOR_C];
  }

  bool get_c_xor_d() const {
    return values_[POLYLFO_SETTING_C_XOR_D];
  }
  
  void Init();

  void freeze() {
    frozen_ = true;
  }

  void thaw() {
    frozen_ = false;
  }

  bool frozen() const {
    return frozen_;
  }

  frames::PolyLfo lfo;
  bool frozen_;

  // ISR update is at 16.666kHz, we don't need it that fast so smooth the values to ~1Khz
  static constexpr int32_t kSmoothing = 16;

  SmoothedValue<int32_t, kSmoothing> cv_freq;
  SmoothedValue<int32_t, kSmoothing> cv_shape;
  SmoothedValue<int32_t, kSmoothing> cv_spread;
  SmoothedValue<int32_t, kSmoothing> cv_coupling;
};

void PolyLfo::Init() {
  InitDefaults();
  lfo.Init();
  frozen_= false;
}

const char* const freq_div_names[frames::POLYLFO_FREQ_DIV_LAST] = {
  "unity", "4/5", "2/3", "3/5", "1/2", "2/5", "1/3", "1/4", "1/5", "1/6", "1/7", "1/8", "1/9", "1/10", "1/11", "1/12", "1/13", "1/14", "1/15", "1/16"
};

SETTINGS_DECLARE(PolyLfo, POLYLFO_SETTING_LAST) {
  { 64, 0, 255, "Coarse freq", NULL, settings::STORAGE_TYPE_U8 },
  { 0, -128, 127, "Fine freq", NULL, settings::STORAGE_TYPE_I16 },
  { 0, 0, 255, "Shape", NULL, settings::STORAGE_TYPE_U8 },
  { 0, -128, 127, "Shape spread", NULL, settings::STORAGE_TYPE_I8 },
  { -1, -128, 127, "Phase/frq sprd", NULL, settings::STORAGE_TYPE_I8 },
  { 0, -128, 127, "Coupling", NULL, settings::STORAGE_TYPE_I8 },
  { frames::POLYLFO_FREQ_DIV_NONE, frames::POLYLFO_FREQ_DIV_NONE, frames::POLYLFO_FREQ_DIV_LAST - 1, "B freq ratio", freq_div_names, settings::STORAGE_TYPE_U8 },
  { frames::POLYLFO_FREQ_DIV_NONE, frames::POLYLFO_FREQ_DIV_NONE, frames::POLYLFO_FREQ_DIV_LAST - 1, "C freq ratio", freq_div_names, settings::STORAGE_TYPE_U8 },
  { frames::POLYLFO_FREQ_DIV_NONE, frames::POLYLFO_FREQ_DIV_NONE, frames::POLYLFO_FREQ_DIV_LAST - 1, "D freq ratio", freq_div_names, settings::STORAGE_TYPE_U8 },
  { 0, 0, 1, "B XOR A", OC::Strings::no_yes, settings::STORAGE_TYPE_U4 },
  { 0, 0, 1, "C XOR B", OC::Strings::no_yes, settings::STORAGE_TYPE_U4 },
  { 0, 0, 1, "D XOR C", OC::Strings::no_yes, settings::STORAGE_TYPE_U4 }, 
};

PolyLfo poly_lfo;
struct {
  int selected_param;
  POLYLFO_SETTINGS left_edit_mode;
  bool editing;
} poly_lfo_state;

void FASTRUN POLYLFO_isr() {

  bool reset_phase = OC::DigitalInputs::clocked<OC::DIGITAL_INPUT_1>();
  bool freeze = OC::DigitalInputs::read_immediate<OC::DIGITAL_INPUT_2>();

  poly_lfo.cv_freq.push(OC::ADC::value<ADC_CHANNEL_1>());
  poly_lfo.cv_shape.push(OC::ADC::value<ADC_CHANNEL_2>());
  poly_lfo.cv_spread.push(OC::ADC::value<ADC_CHANNEL_3>());
  poly_lfo.cv_coupling.push(OC::ADC::value<ADC_CHANNEL_4>());

  // Range in settings is (0-256] so this gets scaled to (0,65535]
  // CV value is 12 bit so also needs scaling

  int32_t freq = SCALE8_16(poly_lfo.get_coarse()) + (poly_lfo.cv_freq.value() * 16) + poly_lfo.get_fine() * 2;
  freq = USAT16(freq);

  int32_t shape = SCALE8_16(poly_lfo.get_shape()) + (poly_lfo.cv_shape.value() * 16);
  poly_lfo.lfo.set_shape(USAT16(shape));

  int32_t spread = SCALE8_16(poly_lfo.get_spread() + 128) + (poly_lfo.cv_spread.value() * 16);
  poly_lfo.lfo.set_spread(USAT16(spread));

  int32_t coupling = SCALE8_16(poly_lfo.get_coupling() + 128) + (poly_lfo.cv_coupling.value() * 16);
  poly_lfo.lfo.set_coupling(USAT16(coupling));

  poly_lfo.lfo.set_shape_spread(SCALE8_16(poly_lfo.get_shape_spread() + 128));

  poly_lfo.lfo.set_freq_div_b(poly_lfo.get_freq_div_b());
  poly_lfo.lfo.set_freq_div_c(poly_lfo.get_freq_div_c());
  poly_lfo.lfo.set_freq_div_d(poly_lfo.get_freq_div_d());

  poly_lfo.lfo.set_a_xor_b(poly_lfo.get_a_xor_b());
  poly_lfo.lfo.set_b_xor_c(poly_lfo.get_b_xor_c());
  poly_lfo.lfo.set_c_xor_d(poly_lfo.get_c_xor_d());

  if (!freeze && !poly_lfo.frozen())
    poly_lfo.lfo.Render(freq, reset_phase);

  DAC::set<DAC_CHANNEL_A>(poly_lfo.lfo.dac_code(0));
  DAC::set<DAC_CHANNEL_B>(poly_lfo.lfo.dac_code(1));
  DAC::set<DAC_CHANNEL_C>(poly_lfo.lfo.dac_code(2));
  DAC::set<DAC_CHANNEL_D>(poly_lfo.lfo.dac_code(3));
}

void POLYLFO_init() {
  poly_lfo_state.selected_param = POLYLFO_SETTING_SHAPE;
  poly_lfo_state.left_edit_mode = POLYLFO_SETTING_COARSE;
  poly_lfo_state.editing = false;
  poly_lfo.Init();
}

size_t POLYLFO_storageSize() {
  return PolyLfo::storageSize();
}

size_t POLYLFO_save(void *storage) {
  return poly_lfo.Save(storage);
}

size_t POLYLFO_restore(const void *storage) {
  return poly_lfo.Restore(storage);
}

void POLYLFO_loop() {
  if (_ENC && (millis() - _BUTTONS_TIMESTAMP > DEBOUNCE)) encoders();
  buttons(BUTTON_TOP);
  buttons(BUTTON_BOTTOM);
  buttons(BUTTON_LEFT);
  buttons(BUTTON_RIGHT);
}

static const size_t kSmallPreviewBufferSize = 32;
uint16_t preview_buffer[kSmallPreviewBufferSize];

void POLYLFO_menu() {
  GRAPHICS_BEGIN_FRAME(false); // no frame, no problem

  graphics.setFont(MENU_DEFAULT_FONT);

  static const weegfx::coord_t kStartX = 0;
  UI_DRAW_TITLE(kStartX);
  graphics.setPrintPos(2, 2);
  graphics.print(PolyLfo::value_attr(poly_lfo_state.left_edit_mode).name);
  graphics.print(poly_lfo.get_value(poly_lfo_state.left_edit_mode), 5);

  int first_visible_param = poly_lfo_state.selected_param - 3;
  if (first_visible_param < POLYLFO_SETTING_SHAPE) first_visible_param = POLYLFO_SETTING_SHAPE ;
  
  UI_START_MENU(kStartX);
  UI_BEGIN_ITEMS_LOOP(kStartX, first_visible_param, POLYLFO_SETTING_LAST, poly_lfo_state.selected_param, 0)
    UI_DRAW_EDITABLE(poly_lfo_state.editing);
    const settings::value_attr &attr = PolyLfo::value_attr(current_item);
    if (current_item != POLYLFO_SETTING_SHAPE) {
      UI_DRAW_SETTING(attr, poly_lfo.get_value(current_item), kUiWideMenuCol1X);
    } else {
      uint16_t shape = poly_lfo.get_value(current_item);
      poly_lfo.lfo.RenderPreview(shape << 8, preview_buffer, kSmallPreviewBufferSize);
      for (weegfx::coord_t x = 0; x < (weegfx::coord_t)kSmallPreviewBufferSize; ++x)
        graphics.setPixel(96 + x, y + 8 - (preview_buffer[x] >> 13));

      UI_DRAW_SETTING(attr, shape,  96 - 32);
    }
  UI_END_ITEMS_LOOP();

  GRAPHICS_END_FRAME();
}

void POLYLFO_screensaver() {
  GRAPHICS_BEGIN_FRAME(false);

  scope_render();

  GRAPHICS_END_FRAME();
}

void POLYLFO_handleEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      encoder[LEFT].setPos(0);
      encoder[RIGHT].setPos(0);
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER:
      break;
  }
}

void POLYLFO_topButton() {
  poly_lfo.change_value(POLYLFO_SETTING_COARSE, 32);
}

void POLYLFO_lowerButton() {
  poly_lfo.change_value(POLYLFO_SETTING_COARSE, -32);
}

void POLYLFO_rightButton() {
  poly_lfo_state.editing = !poly_lfo_state.editing;
}

void POLYLFO_leftButton() {
  if (POLYLFO_SETTING_COARSE == poly_lfo_state.left_edit_mode) {
    poly_lfo_state.left_edit_mode = POLYLFO_SETTING_FINE;
  } else {
    poly_lfo_state.left_edit_mode = POLYLFO_SETTING_COARSE;
  }
}

void POLYLFO_leftButtonLong() {
}

bool POLYLFO_encoders() {
  bool changed = false;
  int value = encoder[LEFT].pos();
  encoder[LEFT].setPos(0);
  if (value) {
    if (poly_lfo.change_value(poly_lfo_state.left_edit_mode, value))
      changed = true;
  }

  value = encoder[RIGHT].pos();
  encoder[RIGHT].setPos(0);
  if (value) {
    if (poly_lfo_state.editing) {
      poly_lfo.change_value(poly_lfo_state.selected_param, value);
    } else {
      value += poly_lfo_state.selected_param;
      CONSTRAIN(value, POLYLFO_SETTING_SHAPE, POLYLFO_SETTING_LAST - 1);
      poly_lfo_state.selected_param = value;
    }
    changed = true;
  }

  return changed;
}

void POLYLFO_debug() {
  graphics.setPrintPos(2, 12);
  graphics.print(poly_lfo.cv_shape.value());
  graphics.print(" ");
  int32_t value = SCALE8_16(poly_lfo.get_shape());
  graphics.print(value);
  graphics.print(" ");
  graphics.print(poly_lfo.cv_shape.value() * 16);
  value += poly_lfo.cv_shape.value() * 16;
  graphics.setPrintPos(2, 22);
  graphics.print(value); graphics.print(" ");
  value = USAT16(value);
  graphics.print(value);
}
