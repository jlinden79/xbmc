/*
 *      Copyright (C) 2005-2016 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kodi; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"

#include "cores/VideoPlayer/VideoRenderers/ColorManager.h"
#include "FileItem.h"
#include "GUIDialogCMSSettings.h"
#include "GUIPassword.h"
#include "ServiceBroker.h"
#include "addons/Skin.h"
#include "cores/VideoPlayer/VideoRenderers/RenderManager.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "guilib/GUIWindowManager.h"
#include "profiles/ProfilesManager.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "settings/lib/SettingsManager.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "video/VideoDatabase.h"
#include "utils/Variant.h"

#include <vector>

#define SETTING_VIDEO_CMSENABLE           "videoscreen.cmsenabled"
#define SETTING_VIDEO_CMSMODE             "videoscreen.cmsmode"
#define SETTING_VIDEO_CMS3DLUT            "videoscreen.cms3dlut"
#define SETTING_VIDEO_CMSWHITEPOINT       "videoscreen.cmswhitepoint"
#define SETTING_VIDEO_CMSPRIMARIES        "videoscreen.cmsprimaries"
#define SETTING_VIDEO_CMSGAMMAMODE        "videoscreen.cmsgammamode"
#define SETTING_VIDEO_CMSGAMMA            "videoscreen.cmsgamma"
#define SETTING_VIDEO_CMSLUTSIZE          "videoscreen.cmslutsize"

CGUIDialogCMSSettings::CGUIDialogCMSSettings()
    : CGUIDialogSettingsManualBase(WINDOW_DIALOG_CMS_OSD_SETTINGS, "DialogSettings.xml")
{ }

CGUIDialogCMSSettings::~CGUIDialogCMSSettings() = default;

void CGUIDialogCMSSettings::SetupView()
{
  CGUIDialogSettingsManualBase::SetupView();

  SetHeading(36560);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_OKAY_BUTTON);
  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_CUSTOM_BUTTON);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_CANCEL_BUTTON, 15067);
}

void CGUIDialogCMSSettings::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

  const std::shared_ptr<CSettingCategory> category = AddCategory("cms", -1);
  if (category == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogCMSSettings: unable to setup settings");
    return;
  }

  // get all necessary setting groups
  const std::shared_ptr<CSettingGroup> groupColorManagement = AddGroup(category);
  if (groupColorManagement == NULL)
  {
    CLog::Log(LOGERROR, "CGUIDialogCMSSettings: unable to setup settings");
    return;
  }

  bool usePopup = g_SkinInfo->HasSkinFile("DialogSlider.xml");

  TranslatableIntegerSettingOptions entries;

  // create "depsCmsEnabled" for settings depending on CMS being enabled
  CSettingDependency dependencyCmsEnabled(SettingDependencyType::Enable, GetSettingsManager());
  dependencyCmsEnabled.Or()
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(SETTING_VIDEO_CMSENABLE, "true", SettingDependencyOperator::Equals, false, GetSettingsManager())));
  SettingDependencies depsCmsEnabled;
  depsCmsEnabled.push_back(dependencyCmsEnabled);

  // create "depsCms3dlut" for 3dlut settings
  CSettingDependency dependencyCms3dlut(SettingDependencyType::Visible, GetSettingsManager());
  dependencyCms3dlut.And()
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(SETTING_VIDEO_CMSMODE, std::to_string(CMS_MODE_3DLUT), SettingDependencyOperator::Equals, false, GetSettingsManager())));
  SettingDependencies depsCms3dlut;
  depsCms3dlut.push_back(dependencyCmsEnabled);
  depsCms3dlut.push_back(dependencyCms3dlut);

  // create "depsCmsIcc" for display settings with icc profile
  CSettingDependency dependencyCmsIcc(SettingDependencyType::Visible, GetSettingsManager());
  dependencyCmsIcc.And()
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(SETTING_VIDEO_CMSMODE, std::to_string(CMS_MODE_PROFILE), SettingDependencyOperator::Equals, false, GetSettingsManager())));
  SettingDependencies depsCmsIcc;
  depsCmsIcc.push_back(dependencyCmsEnabled);
  depsCmsIcc.push_back(dependencyCmsIcc);

  // create "depsCmsGamma" for effective gamma adjustment (not available with bt.1886)
  CSettingDependency dependencyCmsGamma(SettingDependencyType::Visible, GetSettingsManager());
  dependencyCmsGamma.And()
    ->Add(CSettingDependencyConditionPtr(new CSettingDependencyCondition(SETTING_VIDEO_CMSGAMMAMODE, std::to_string(CMS_TRC_BT1886), SettingDependencyOperator::Equals, true, GetSettingsManager())));
  SettingDependencies depsCmsGamma;
  depsCmsGamma.push_back(dependencyCmsEnabled);
  depsCmsGamma.push_back(dependencyCmsIcc);
  depsCmsGamma.push_back(dependencyCmsGamma);

  // color management settings
  AddToggle(groupColorManagement, SETTING_VIDEO_CMSENABLE, 36560, SettingLevel::Basic, CServiceBroker::GetSettings().GetBool(SETTING_VIDEO_CMSENABLE));

  int currentMode = CServiceBroker::GetSettings().GetInt(SETTING_VIDEO_CMSMODE);
  entries.clear();
  // entries.push_back(std::make_pair(16039, CMS_MODE_OFF)); // FIXME: get from CMS class
  entries.push_back(std::make_pair(36580, CMS_MODE_3DLUT));
#ifdef HAVE_LCMS2
  entries.push_back(std::make_pair(36581, CMS_MODE_PROFILE));
#endif
  std::shared_ptr<CSettingInt> settingCmsMode = AddSpinner(groupColorManagement, SETTING_VIDEO_CMSMODE, 36562, SettingLevel::Basic, currentMode, entries);
  settingCmsMode->SetDependencies(depsCmsEnabled);

  std::string current3dLUT = CServiceBroker::GetSettings().GetString(SETTING_VIDEO_CMS3DLUT);
  std::shared_ptr<CSettingString> settingCms3dlut = AddList(groupColorManagement, SETTING_VIDEO_CMS3DLUT, 36564, SettingLevel::Basic, current3dLUT, Cms3dLutsFiller, 36564);
  settingCms3dlut->SetDependencies(depsCms3dlut);

  // display settings
  int currentWhitepoint = CServiceBroker::GetSettings().GetInt(SETTING_VIDEO_CMSWHITEPOINT);
  entries.clear();
  entries.push_back(std::make_pair(36586, CMS_WHITEPOINT_D65));
  entries.push_back(std::make_pair(36587, CMS_WHITEPOINT_D93));
  std::shared_ptr<CSettingInt> settingCmsWhitepoint = AddSpinner(groupColorManagement, SETTING_VIDEO_CMSWHITEPOINT, 36568, SettingLevel::Basic, currentWhitepoint, entries);
  settingCmsWhitepoint->SetDependencies(depsCmsIcc);

  int currentPrimaries = CServiceBroker::GetSettings().GetInt(SETTING_VIDEO_CMSPRIMARIES);
  entries.clear();
  entries.push_back(std::make_pair(36588, CMS_PRIMARIES_AUTO));
  entries.push_back(std::make_pair(36589, CMS_PRIMARIES_BT709));
  entries.push_back(std::make_pair(36579, CMS_PRIMARIES_BT2020));
  entries.push_back(std::make_pair(36590, CMS_PRIMARIES_170M));
  entries.push_back(std::make_pair(36591, CMS_PRIMARIES_BT470M));
  entries.push_back(std::make_pair(36592, CMS_PRIMARIES_BT470BG));
  entries.push_back(std::make_pair(36593, CMS_PRIMARIES_240M));
  std::shared_ptr<CSettingInt> settingCmsPrimaries = AddSpinner(groupColorManagement, SETTING_VIDEO_CMSPRIMARIES, 36570, SettingLevel::Basic, currentPrimaries, entries);
  settingCmsPrimaries->SetDependencies(depsCmsIcc);

  int currentGammaMode = CServiceBroker::GetSettings().GetInt(SETTING_VIDEO_CMSGAMMAMODE);
  entries.clear();
  entries.push_back(std::make_pair(36582, CMS_TRC_BT1886));
  entries.push_back(std::make_pair(36583, CMS_TRC_INPUT_OFFSET));
  entries.push_back(std::make_pair(36584, CMS_TRC_OUTPUT_OFFSET));
  entries.push_back(std::make_pair(36585, CMS_TRC_ABSOLUTE));
  std::shared_ptr<CSettingInt> settingCmsGammaMode = AddSpinner(groupColorManagement, SETTING_VIDEO_CMSGAMMAMODE, 36572, SettingLevel::Basic, currentGammaMode, entries);
  settingCmsGammaMode->SetDependencies(depsCmsIcc);

  float currentGamma = CServiceBroker::GetSettings().GetInt(SETTING_VIDEO_CMSGAMMA)/100.0f;
  if (currentGamma == 0.0) currentGamma = 2.20;
  std::shared_ptr<CSettingNumber> settingCmsGamma = AddSlider(groupColorManagement, SETTING_VIDEO_CMSGAMMA, 36574, SettingLevel::Basic, currentGamma, 36597, 1.6, 0.05, 2.8, 36574, usePopup);
  settingCmsGamma->SetDependencies(depsCmsGamma);

  int currentLutSize = CServiceBroker::GetSettings().GetInt(SETTING_VIDEO_CMSLUTSIZE);
  entries.clear();
  entries.push_back(std::make_pair(36594, 4));
  entries.push_back(std::make_pair(36595, 6));
  entries.push_back(std::make_pair(36596, 8));
  std::shared_ptr<CSettingInt> settingCmsLutSize = AddSpinner(groupColorManagement, SETTING_VIDEO_CMSLUTSIZE, 36576, SettingLevel::Basic, currentLutSize, entries);
  settingCmsLutSize->SetDependencies(depsCmsIcc);
}

void CGUIDialogCMSSettings::OnSettingChanged(std::shared_ptr<const CSetting> setting)
{
  if (setting == NULL)
    return;

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);

  const std::string &settingId = setting->GetId();
  if (settingId == SETTING_VIDEO_CMSENABLE)
    CServiceBroker::GetSettings().SetBool(SETTING_VIDEO_CMSENABLE, (std::static_pointer_cast<const CSettingBool>(setting)->GetValue()));
  else if (settingId == SETTING_VIDEO_CMSMODE)
    CServiceBroker::GetSettings().SetInt(SETTING_VIDEO_CMSMODE, static_cast<int>(std::static_pointer_cast<const CSettingInt>(setting)->GetValue()));
  else if (settingId == SETTING_VIDEO_CMS3DLUT)
    CServiceBroker::GetSettings().SetString(SETTING_VIDEO_CMS3DLUT, static_cast<std::string>(std::static_pointer_cast<const CSettingString>(setting)->GetValue()));
  else if (settingId == SETTING_VIDEO_CMSWHITEPOINT)
    CServiceBroker::GetSettings().SetInt(SETTING_VIDEO_CMSWHITEPOINT, static_cast<int>(std::static_pointer_cast<const CSettingInt>(setting)->GetValue()));
  else if (settingId == SETTING_VIDEO_CMSPRIMARIES)
    CServiceBroker::GetSettings().SetInt(SETTING_VIDEO_CMSPRIMARIES, static_cast<int>(std::static_pointer_cast<const CSettingInt>(setting)->GetValue()));
  else if (settingId == SETTING_VIDEO_CMSGAMMAMODE)
    CServiceBroker::GetSettings().SetInt(SETTING_VIDEO_CMSGAMMAMODE, static_cast<int>(std::static_pointer_cast<const CSettingInt>(setting)->GetValue()));
  else if (settingId == SETTING_VIDEO_CMSGAMMA)
    CServiceBroker::GetSettings().SetInt(SETTING_VIDEO_CMSGAMMA, static_cast<float>(std::static_pointer_cast<const CSettingNumber>(setting)->GetValue())*100);
  else if (settingId == SETTING_VIDEO_CMSLUTSIZE)
    CServiceBroker::GetSettings().SetInt(SETTING_VIDEO_CMSLUTSIZE, static_cast<int>(std::static_pointer_cast<const CSettingInt>(setting)->GetValue()));
}

bool CGUIDialogCMSSettings::OnBack(int actionID)
{
  Save();
  return CGUIDialogSettingsBase::OnBack(actionID);
}

void CGUIDialogCMSSettings::Save()
{
  CLog::Log(LOGINFO, "CGUIDialogCMSSettings: Save() called");
  CServiceBroker::GetSettings().Save();
}

void CGUIDialogCMSSettings::Cms3dLutsFiller(
    SettingConstPtr setting,
    std::vector< std::pair<std::string, std::string> > &list,
    std::string &current,
    void *data)
{
  // get 3dLut directory from settings
  CFileItemList items;

  // list .3dlut files
  std::string current3dlut = CServiceBroker::GetSettings().GetString(SETTING_VIDEO_CMS3DLUT);
  if (!current3dlut.empty())
    current3dlut = URIUtils::GetDirectory(current3dlut);
  XFILE::CDirectory::GetDirectory(current3dlut, items, ".3dlut");

  for (int i = 0; i < items.Size(); i++)
  {
    list.push_back(make_pair(items[i]->GetLabel(), items[i]->GetPath()));
  }
}
