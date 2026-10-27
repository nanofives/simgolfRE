#include "input_map.h"

namespace portapp {

namespace {

constexpr uint32_t kVkShift = 0x10, kVkControl = 0x11, kVkMenu = 0x12;
constexpr uint32_t kMapVkToChar = 2;       // MAPVK_VK_TO_CHAR: the maptype the corpus' MAPVK records carry
constexpr int32_t kCaptionHeight = 23;     // YFrameOffset control only

int32_t sx16(uint32_t v) {
  v &= 0xffffu;
  return static_cast<int32_t>(v >= 0x8000u ? v - 0x10000u : v);
}

int64_t floor_div(int64_t a, int64_t b) {  // b > 0
  int64_t q = a / b;
  if ((a % b != 0) && (a < 0)) --q;
  return q;
}

Step call(uint8_t handler, int32_t a0, int32_t a1) {
  Step s;
  s.kind = Step::Kind::Call;
  s.handler = handler;
  s.a0 = a0;
  s.a1 = a1;
  return s;
}

Step read_key(ReadApi api, uint32_t vk, uint32_t result) {
  Step s;
  s.kind = Step::Kind::Read;
  s.api = api;
  s.vk = vk;
  s.result = result;
  return s;
}

}  // namespace

Point Translator::to_logical(Point c) const {
  Point p;
  if (defect_ == Defect::NoViewport) return c;
  const int32_t w = vp_.client_w > 0 ? vp_.client_w : kLogicalW;
  const int32_t h = vp_.client_h > 0 ? vp_.client_h : kLogicalH;
  p.x = static_cast<int32_t>(floor_div(static_cast<int64_t>(c.x - vp_.origin_x) * kLogicalW, w));
  p.y = static_cast<int32_t>(floor_div(static_cast<int64_t>(c.y - vp_.origin_y) * kLogicalH, h));
  if (defect_ == Defect::YFlip) p.y = kLogicalH - 1 - p.y;
  if (defect_ == Defect::YFrameOffset) p.y += kCaptionHeight;
  return p;
}

Point Translator::lparam_point(uint32_t lp) const {
  Point c;
  c.x = sx16(lp);
  c.y = sx16(lp >> 16);
  return to_logical(c);
}

Evidence Translator::evidence_of(uint32_t msg) {
  switch (msg) {
    case kWmKeyDown: case kWmKeyUp: case kWmChar:
    case kWmMouseMove: case kWmLButtonDown: case kWmLButtonUp: case kWmRButtonDown: case kWmRButtonUp:
      return Evidence::Corpus;
    case kWmSysKeyDown: case kWmSysKeyUp: case kWmLButtonDblClk: case kWmRButtonDblClk:
    case 0x20a: case 0x207: case 0x208: case 0x209:  // wheel, middle button
    case 0x103: case 0x106: case 0x107:              // WM_DEADCHAR, WM_SYSCHAR, WM_SYSDEADCHAR
      return Evidence::Unmeasured;
    default:
      return Evidence::NotInput;
  }
}

Translation Translator::translate(uint32_t msg, uint32_t wp, uint32_t lp, HostInput &host) {
  Translation out;
  out.evidence = evidence_of(msg);
  std::vector<Step> &st = out.steps;

  uint32_t m = msg;
  if (m == kWmSysKeyDown) m = kWmKeyDown;  // jgld's key handler serves all four key messages alike (onKey)
  if (m == kWmSysKeyUp) m = kWmKeyUp;
  if (m == kWmLButtonDblClk) m = kWmLButtonDown;  // jgld hands the sink a double-click flag and no root handler tests it
  if (m == kWmRButtonDblClk) m = kWmRButtonDown;

  if (defect_ == Defect::SwapButtons) {
    if (m == kWmLButtonDown) m = kWmRButtonDown;
    else if (m == kWmRButtonDown) m = kWmLButtonDown;
    else if (m == kWmLButtonUp) m = kWmRButtonUp;
    else if (m == kWmRButtonUp) m = kWmLButtonUp;
  }

  switch (m) {
    case kWmKeyDown: {
      // uiKeyEvent reads the three modifier states first, then the key's character is looked up. It ORs 0x10000 (Shift),
      // 0x20000 (Ctrl), 0x40000 (Alt) into the code it passes on when GetKeyState's high bit is set (0x00495918..0x00495943;
      // roadmap S7.4's cross-check measured onKey(0, 0x10010), (0, 0x20011), (0, 0x40012) and onChar(0x40058) in the
      // original, log/roadmap/S7/4_notes.md; no corpus trace has a modifier down at a key-down, U-1611)
      uint32_t mods = 0;
      for (uint32_t vk : {kVkShift, kVkControl, kVkMenu}) {
        const uint16_t s = host.get_key_state(vk);
        st.push_back(read_key(ReadApi::GetKeyState, vk, s));
        if (s & 0x8000u) mods |= vk == kVkShift ? 0x10000u : vk == kVkControl ? 0x20000u : 0x40000u;
      }
      const uint32_t code = wp | mods;
      const uint32_t mapped = host.map_virtual_key(wp, kMapVkToChar);
      Step r;
      r.kind = Step::Kind::Read;
      r.api = ReadApi::MapVirtualKey;
      r.vk = wp;
      r.map_type = kMapVkToChar;
      r.result = mapped;
      st.push_back(r);
      last_vk_ = code;  // a WM_CHAR's code is the key-down's code with its modifier bits (S7.4: onKey(0x61, 0x10041) with Shift)
      if (defect_ != Defect::IgnoreFocusTake && host.focus_child_takes(kWmKeyDown, wp)) {
        st.push_back(call(kFocusKeyDown, static_cast<int32_t>(wp), 0));
        break;
      }
      Step c;
      if (defect_ == Defect::SwapKeyHandlers) {
        c = call(kOnKey, static_cast<int32_t>(wp), static_cast<int32_t>(wp));
        c.t = host.now_ms();
      } else if (mapped != 0 || defect_ == Defect::AlwaysOnChar) {
        c = call(kOnChar, static_cast<int32_t>(code), 0);
      } else {
        c = call(kOnKey, 0, static_cast<int32_t>(code));
        c.t = host.now_ms();
      }
      st.push_back(c);
      break;
    }
    case kWmKeyUp: {
      for (uint32_t vk : {kVkShift, kVkControl, kVkMenu}) st.push_back(read_key(ReadApi::GetKeyState, vk, host.get_key_state(vk)));
      st.push_back(call(kOnChar0, 0, 0));
      break;
    }
    case kWmChar: {
      if (defect_ != Defect::IgnoreFocusTake && host.focus_child_takes(kWmChar, wp)) {
        st.push_back(call(kFocusChar, static_cast<int32_t>(wp), 0));
        break;
      }
      // jgld's onChar hands the sink the character's low byte; the sink's handler gets the key-down's code beside it
      const int32_t ch = static_cast<int32_t>(wp & 0xffu);
      Step c;
      if (defect_ == Defect::SwapKeyHandlers) {
        c = call(kOnChar, ch, 0);
      } else {
        c = call(kOnKey, ch, static_cast<int32_t>(defect_ == Defect::StaleCode ? wp & 0xffu : last_vk_));
        c.t = host.now_ms();
      }
      st.push_back(c);
      break;
    }
    case kWmMouseMove: {
      // the window procedure reads the cursor once per WM_MOUSEMOVE; no root handler runs
      Step r;
      r.kind = Step::Kind::Read;
      r.api = ReadApi::Cursor;
      r.pt = to_logical(host.cursor());
      st.push_back(r);
      break;
    }
    case kWmLButtonDown:
    case kWmRButtonDown: {
      const Point p = lparam_point(lp);
      if (msg == kWmLButtonDblClk || msg == kWmRButtonDblClk) {
        // a double click: rootWin_onPress1b 0x004657e0 / onPress2b 0x00465840 (handlers 10 / 11: the click point, the
        // button word 0x00822d68 = 1 / 2, flags 0x00822d78 / 0x00822d74 = 1; no key or clock read) and then the press
        // handler, the order roadmap S7.4's cross-check measured in the original for WM_LBUTTONDBLCLK (U-1612)
        st.push_back(call(msg == kWmLButtonDblClk ? 10 : 11, p.x, p.y));
      }
      Step c = call(m == kWmLButtonDown ? kOnPoint : kOnPoint2, p.x, p.y);
      c.t = host.now_ms();
      st.push_back(c);
      break;
    }
    case kWmLButtonUp:
    case kWmRButtonUp: {
      // the button's release is the click: the press handler reads Ctrl and Shift, then the unpoint handler follows
      const Point p = lparam_point(lp);
      uint16_t ctrl = host.get_async_key_state(kVkControl);
      uint16_t shift = host.get_async_key_state(kVkShift);
      if (defect_ == Defect::DropCtrl) ctrl = 0;
      if (defect_ == Defect::DropShift) shift = 0;
      Step c = call(m == kWmLButtonUp ? kOnPress1 : kOnPress2, p.x, p.y);
      c.async_ctrl = ctrl;
      c.async_shift = shift;
      st.push_back(c);
      st.push_back(read_key(ReadApi::GetAsyncKeyState, kVkControl, ctrl));
      st.push_back(read_key(ReadApi::GetAsyncKeyState, kVkShift, shift));
      Step u = call(m == kWmLButtonUp ? kOnUnpoint : kOnUnpoint2, 0, 0);
      u.t = host.now_ms();
      st.push_back(u);
      break;
    }
    default:
      break;
  }
  return out;
}

}  // namespace portapp
