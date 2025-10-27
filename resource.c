#include "resource.h"

void showAlertDynamic(uint8_t alert_type, Str255 message) {
  ParamText(message, "\p", "\p", "\p");
  switch(alert_type) {
    case kAlertPlainAlert  :        Alert(RSRC_ALRT_DEFAULT, nil); break;
    case kAlertStopAlert   :    StopAlert(RSRC_ALRT_DEFAULT, nil); break;
    case kAlertNoteAlert   :    NoteAlert(RSRC_ALRT_DEFAULT, nil); break;
    case kAlertCautionAlert: CautionAlert(RSRC_ALRT_DEFAULT, nil); break;
  }
}

void showAlertStatic(uint8_t alert_type, uint16_t rsrc_strn_id, uint16_t rsrc_strn_index) {
  Str255 message;
  GetIndString(message, rsrc_strn_id, rsrc_strn_index);
  ParamText(message, "\p", "\p", "\p");
  switch(alert_type) {
    case kAlertPlainAlert  :        Alert(RSRC_ALRT_DEFAULT, nil); break;
    case kAlertStopAlert   :    StopAlert(RSRC_ALRT_DEFAULT, nil); break;
    case kAlertNoteAlert   :    NoteAlert(RSRC_ALRT_DEFAULT, nil); break;
    case kAlertCautionAlert: CautionAlert(RSRC_ALRT_DEFAULT, nil); break;
  }
}
