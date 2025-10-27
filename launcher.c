#pragma c9x on
#include "launcher.h"
#include "resource.h"
#include <string.h> // memcpy

//==================================================================
// globals
uint8_t g_alias_count;
FSSpec  g_alias_entry[255];
Str63   g_alias_entry_name[255];
bool    g_alias_matched;
uint8_t g_alias_match_index;
FSSpec  g_alias_folder;
LauncherMatchUpdate g_match_update;

//==================================================================
// utilities
bool getAliasFolder();
bool scanAliasFolder();
bool launchFSSpec(FSSpec* fs_spec);
void toLower63(Str63 input, Str63 output);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool getAliasFolder(Str255 app_name) {
  int16_t sys_prefs_vref;
  int32_t sys_prefs_did;
  int32_t alias_folder_did;
  OSErr err;

  // get System Preferences folder
  err = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder, &sys_prefs_vref, &sys_prefs_did);
  if(err != noErr) {
    showAlertStatic(kAlertStopAlert, RSRC_STRN_ERRORS, STRN_ERRORS_ALIASFOLDER);
    return false;
  }

  // get/create our preferences folder
  err = FSMakeFSSpec(sys_prefs_vref, sys_prefs_did, app_name, &g_alias_folder);
  if(err != noErr) {
    err = FSpDirCreate(&g_alias_folder, smSystemScript, &alias_folder_did);
  }
  if(err != noErr) {
    showAlertStatic(kAlertStopAlert, RSRC_STRN_ERRORS, STRN_ERRORS_ALIASFOLDER);
    return false;
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool scanAliasFolder() {
  CInfoPBRec pb_rec;
  Str63      filename;
  uint32_t   alias_folder_did;
  uint16_t   index;
  OSErr      err;

  // first, we need the directory id of our alias folder
  pb_rec.dirInfo.ioVRefNum   = g_alias_folder.vRefNum;
  pb_rec.dirInfo.ioDrDirID   = g_alias_folder.parID;
  pb_rec.dirInfo.ioNamePtr   = g_alias_folder.name;
  pb_rec.dirInfo.ioFDirIndex = 0;
  err = PBGetCatInfoSync(&pb_rec);
  if(err != noErr) {
    showAlertStatic(kAlertStopAlert, RSRC_STRN_ERRORS, STRN_ERRORS_ALIASFOLDER);
    return false;
  }
  alias_folder_did = pb_rec.dirInfo.ioDrDirID;

  // now, we can iterate through our alias folder by dir_index (1-indexed for children)
  g_alias_count = 0;
  index = 0;
  while(g_alias_count < 255) {
    index += 1;
    pb_rec.hFileInfo.ioNamePtr   = filename;
    pb_rec.hFileInfo.ioVRefNum   = g_alias_folder.vRefNum;
    pb_rec.hFileInfo.ioDirID     = alias_folder_did;
    pb_rec.hFileInfo.ioFDirIndex = index;
    err = PBGetCatInfoSync(&pb_rec);
    // we'll get a `fnfErr` at the end of the directory (and otherwise skip errors)
    if(err != noErr) { break; }
    // skip invisible files
    if((pb_rec.hFileInfo.ioFlAttrib & kioFlAttribLockedMask) != 0) { continue; }

    // add this fsspec to g_alias_entry (and a toLower version for matching against)
    g_alias_entry[g_alias_count].vRefNum = g_alias_folder.vRefNum;
    g_alias_entry[g_alias_count].parID   = alias_folder_did;
    memcpy(g_alias_entry[g_alias_count].name, filename, 64);
    toLower63(filename, g_alias_entry_name[g_alias_count]);

    g_alias_count += 1;
  }

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool launchFSSpec(FSSpec* fs_spec) {
  AEDesc event_desc  = { 0 };
  AEDesc fsspec_desc = { 0 };
  long finder_app_signature = 'MACS';
  AppleEvent event, reply;
  OSErr err;

  // ['misc', 'actv'] is "activate", to activate finder
  // ['misc','mvis'] is "reveal", finder will show this item selected (in parent directory)
  // ['FNDR', 'sope'] is "open", finder will open the specified directory (or whatever)

  // activate finder
  AECreateDesc(typeApplSignature, (Ptr)(&finder_app_signature), (Size)sizeof(finder_app_signature), &event_desc);
  AECreateAppleEvent('misc', 'actv', &event_desc, kAutoGenerateReturnID, 1L, &event);
  err = AESend(&event, &reply, kAENoReply+kAECanInteract+kAECanSwitchLayer, kAENormalPriority, kAEDefaultTimeout, nil, nil);
  if(err != noErr) { return false; }

  // tell finder to open FSSpec
  AECreateAppleEvent('FNDR', 'sope', &event_desc, kAutoGenerateReturnID, 1L, &event);
  AECreateDesc(typeFSS, (Ptr)fs_spec, (Size)sizeof(FSSpec), &fsspec_desc);
  AEPutParamDesc(&event, keyDirectObject, &fsspec_desc);
  err = AESend(&event, &reply, kAENoReply+kAECanInteract+kAECanSwitchLayer, kAENormalPriority, kAEDefaultTimeout, nil, nil);
  if(err != noErr) { return false; }

  // cleanup
  AEDisposeDesc(&fsspec_desc);
  AEDisposeDesc(&event_desc);

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void toLower63(Str63 input, Str63 output) {
  uint8_t length = input[0];
  uint8_t index;

  output[0] = input[0];
  for(index=1; index<=length; ++index) {
    if((input[index] >= 'A') && (input[index] <= 'Z')) {
      output[index] = 'a' + (input[index] - 'A');
    } else {
      output[index] = input[index];
    }
  }
}

//==================================================================
// External Interface
bool launcherInit(Str255 app_name, LauncherMatchUpdate fn_update) {
  g_alias_count   = 0;
  g_alias_matched = false;
  g_match_update  = fn_update;
  if(app_name[0] < 1)                   { return false; }
  if(fn_update == NULL)                 { return false; }
  if(getAliasFolder(app_name) == false) { return false; }
  if(scanAliasFolder()        == false) { return false; }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool launcherActivate() {
  if(g_alias_matched == false) {
    SysBeep(100);
    return false;
  }
  return launchFSSpec(&(g_alias_entry[g_alias_match_index]));
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool launcherOpenFolder() {
  return launchFSSpec(&g_alias_folder);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void launcherMatch(char* text, uint8_t length) {
  bool    new_match_found;
  uint8_t match_index  = 0;
  uint8_t match_length = 0;
  uint8_t index;

  /* TODO
     in the future, i'd like to split up alias filenames by whitespace into words
     and add each word to a table with a corresponding alias_number index
     then match by individual words, rather than filenames
     (because this method requires the match to be from the very beginning, not mid-filename)
  */

  // compare input text against all alias names, find the longest match
  if(length > 0) {
    for(index=0; index<g_alias_count; ++index) {
      uint8_t match_length_current = 0;
      uint8_t name_length = g_alias_entry_name[index][0];
      char* name_cstring = (char*)(&(g_alias_entry_name[index][1]));

      if(length > name_length) { continue; }

      while(
        (match_length_current < length) &&
        (
          name_cstring[match_length_current] ==
          text[match_length_current]
        )
      ) { ++match_length_current; }

      if(
          (match_length_current > match_length) &&
          (match_length_current == length)
      ) {
        match_length = match_length_current;
        match_index  = index;
      }
    }
  }

  new_match_found = (match_length > 0);

  // no match found, where previously there was
  if((new_match_found == false) && (g_alias_matched == true)) {
    g_alias_matched     = false;
    g_alias_match_index = 0;
    g_match_update(NULL);
  }

  // match found
  if(new_match_found == true) {
    // either match when there was none, or match changed
    if((g_alias_matched == false) || (match_index != g_alias_match_index)) {
      g_alias_matched = true;
      g_alias_match_index = match_index;
      g_match_update(&(g_alias_entry[match_index]));
    }
  }
}

//==================================================================
