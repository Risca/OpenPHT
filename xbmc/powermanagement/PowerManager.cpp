 /*
 *      Copyright (C) 2005-2012 Team XBMC
 *      http://www.xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "PowerManager.h"

#include <list>
#include <memory>

#include "Application.h"
#include "cores/AudioEngine/AEFactory.h"
#include "dialogs/GUIDialogBusy.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/LocalizeStrings.h"
#include "interfaces/AnnouncementManager.h"
#include "interfaces/Builtins.h"
#include "settings/GUISettings.h"
#include "system.h"
#include "utils/log.h"
#include "utils/Weather.h"
#include "windowing/WindowingFactory.h"

#ifdef HAS_LCD
#include "utils/LCDFactory.h"
#endif

#if defined(TARGET_DARWIN)
#include "osx/CocoaPowerSyscall.h"
#elif defined(TARGET_ANDROID)
#include "android/AndroidPowerSyscall.h"
#elif defined(TARGET_POSIX)
#include "linux/FallbackPowerSyscall.h"
#if defined(HAS_DBUS)
#include "linux/ConsoleUPowerSyscall.h"
#include "linux/ConsoleDeviceKitPowerSyscall.h"
#include "linux/LogindUPowerSyscall.h"
#include "linux/UPowerSyscall.h"
#endif // HAS_DBUS
#elif defined(TARGET_WINDOWS)
#include "powermanagement/windows/Win32PowerSyscall.h"
extern HWND g_hWnd;
#endif

#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>

using namespace ANNOUNCEMENT;

CPowerManager g_powerManager;

template<class T>
IPowerSyscall* PowerSyscallFactory()
{
  return new T;
}

CPowerManager::CPowerManager()
{
  m_instance = NULL;
}

CPowerManager::~CPowerManager()
{
  delete m_instance;
}

void CPowerManager::Initialize()
{
  SAFE_DELETE(m_instance);

#if defined(TARGET_DARWIN)
  m_instance = new CCocoaPowerSyscall();
#elif defined(TARGET_ANDROID)
  m_instance = new CAndroidPowerSyscall();
#elif defined(TARGET_POSIX)
#if defined(HAS_DBUS)
  IPowerSyscall *bestPowerManager;
  int bestCount = -1;
  int currCount = -1;
  
  typedef std::list< std::pair< boost::function<bool()>, boost::function<IPowerSyscall*()> > > PowerList;
  PowerList powerManagers;
  powerManagers.push_back(std::make_pair(
    CConsoleUPowerSyscall::HasConsoleKitAndUPower, PowerSyscallFactory<CConsoleUPowerSyscall>));
  powerManagers.push_back(std::make_pair(
    CConsoleDeviceKitPowerSyscall::HasDeviceConsoleKit, PowerSyscallFactory<CConsoleDeviceKitPowerSyscall>));
  powerManagers.push_back(std::make_pair(
    CLogindUPowerSyscall::HasLogind, PowerSyscallFactory<CLogindUPowerSyscall>));
  powerManagers.push_back(std::make_pair(
    CUPowerSyscall::HasUPower, PowerSyscallFactory<CUPowerSyscall>));

  for (PowerList::const_iterator powerManager = powerManagers.begin();
       powerManager != powerManagers.end(); powerManager++)
  {
    if (powerManager->first())
    {
      IPowerSyscall *currPowerManager = powerManager->second();
      currCount = currPowerManager->CountPowerFeatures();
      if (currCount > bestCount)
      {
        bestCount = currCount;
        if (bestPowerManager) delete bestPowerManager;
        bestPowerManager = currPowerManager;
      }
      else {
        delete currPowerManager;
      }
      if (bestCount == IPowerSyscall::MAX_COUNT_POWER_FEATURES)
        break;
    }
  }
  if (bestPowerManager)
    m_instance = bestPowerManager;
  else
#endif // HAS_DBUS
    m_instance = new CFallbackPowerSyscall();
#elif defined(TARGET_WINDOWS)
  m_instance = new CWin32PowerSyscall();
#endif

  if (m_instance == NULL)
    m_instance = new CNullPowerSyscall();
}

void CPowerManager::SetDefaults()
{
  int defaultShutdown = g_guiSettings.GetInt("powermanagement.shutdownstate");

  switch (defaultShutdown)
  {
    case POWERSTATE_QUIT:
    case POWERSTATE_MINIMIZE:
      // assume we can shutdown if --standalone is passed
      if (g_application.IsStandAlone())
        defaultShutdown = POWERSTATE_SHUTDOWN;
    break;
    case POWERSTATE_HIBERNATE:
      if (!g_powerManager.CanHibernate())
      {
        if (g_powerManager.CanSuspend())
          defaultShutdown = POWERSTATE_SUSPEND;
        else
          defaultShutdown = g_powerManager.CanPowerdown() ? POWERSTATE_SHUTDOWN : POWERSTATE_QUIT;
      }
    break;
    case POWERSTATE_SUSPEND:
      if (!g_powerManager.CanSuspend())
      {
        if (g_powerManager.CanHibernate())
          defaultShutdown = POWERSTATE_HIBERNATE;
        else
          defaultShutdown = g_powerManager.CanPowerdown() ? POWERSTATE_SHUTDOWN : POWERSTATE_QUIT;
      }
    break;
    case POWERSTATE_SHUTDOWN:
      if (!g_powerManager.CanPowerdown())
      {
        if (g_powerManager.CanSuspend())
          defaultShutdown = POWERSTATE_SUSPEND;
        else
          defaultShutdown = g_powerManager.CanHibernate() ? POWERSTATE_HIBERNATE : POWERSTATE_QUIT;
      }
    break;
  }

  g_guiSettings.SetInt("powermanagement.shutdownstate", defaultShutdown);
}

bool CPowerManager::Powerdown()
{
  if (CanPowerdown() && m_instance->Powerdown())
  {
    CGUIDialogBusy* dialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
    if (dialog)
      dialog->Show();

    return true;
  }

  return false;
}

bool CPowerManager::Suspend()
{
  return (CanSuspend() && m_instance->Suspend());
}

bool CPowerManager::Hibernate()
{
  return (CanHibernate() && m_instance->Hibernate());
}

bool CPowerManager::Reboot()
{
  bool success = CanReboot() ? m_instance->Reboot() : false;

  if (success)
  {
    CAnnouncementManager::Announce(System, "xbmc", "OnRestart");

    CGUIDialogBusy* dialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
    if (dialog)
      dialog->Show();
  }

  return success;
}

bool CPowerManager::CanPowerdown()
{
  return m_instance->CanPowerdown();
}
bool CPowerManager::CanSuspend()
{
  return m_instance->CanSuspend();
}
bool CPowerManager::CanHibernate()
{
  return m_instance->CanHibernate();
}
bool CPowerManager::CanReboot()
{
  return m_instance->CanReboot();
}
int CPowerManager::BatteryLevel()
{
  return m_instance->BatteryLevel();
}
void CPowerManager::ProcessEvents()
{
  static int nesting = 0;

  if (++nesting == 1)
    m_instance->PumpPowerEvents(this);

  nesting--;
}

void CPowerManager::OnSleep()
{
  CAnnouncementManager::Announce(System, "xbmc", "OnSleep");

  CGUIDialogBusy* dialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
  if (dialog)
    dialog->Show();

  CLog::Log(LOGNOTICE, "%s: Running sleep jobs", __FUNCTION__);

#ifdef HAS_LCD
  g_lcd->SetBackLight(0);
#endif

  // stop lirc
#if defined(HAS_LIRC) || defined(HAS_IRSERVERSUITE)
  CLog::Log(LOGNOTICE, "%s: Stopping lirc", __FUNCTION__);
  CBuiltins::Execute("LIRC.Stop");
#endif

#ifndef __PLEX__
  g_application.SaveFileState(true);
#endif
  g_application.StopPlaying();
  g_application.StopShutdownTimer();
  g_application.StopScreenSaverTimer();
  CAEFactory::Suspend();
}

void CPowerManager::OnWake()
{
  CLog::Log(LOGNOTICE, "%s: Running resume jobs", __FUNCTION__);

  // reset out timers
  g_application.ResetShutdownTimers();

  CGUIDialogBusy* dialog = (CGUIDialogBusy*)g_windowManager.GetWindow(WINDOW_DIALOG_BUSY);
  if (dialog)
    dialog->Close();

#if defined(HAS_SDL) || defined(TARGET_WINDOWS)
  if (g_Windowing.IsFullScreen())
  {
#if defined(TARGET_WINDOWS)
    ShowWindow(g_hWnd,SW_RESTORE);
    SetForegroundWindow(g_hWnd);
#endif
  }
  g_application.ResetScreenSaver();
#endif

  // restart lirc
#if defined(HAS_LIRC) || defined(HAS_IRSERVERSUITE)
  CLog::Log(LOGNOTICE, "%s: Restarting lirc", __FUNCTION__);
  CBuiltins::Execute("LIRC.Start");
#endif

  // restart and undim lcd
#ifdef HAS_LCD
  CLog::Log(LOGNOTICE, "%s: Restarting lcd", __FUNCTION__);
  g_lcd->SetBackLight(1);
  g_lcd->Stop();
  g_lcd->Initialize();
#endif

  CAEFactory::Resume();
  g_application.UpdateLibraries();
  g_weatherManager.Refresh();

  /* PLEX */
  g_application.OnWakeUp();
  /* END PLEX */

  CAnnouncementManager::Announce(System, "xbmc", "OnWake");
}

void CPowerManager::OnLowBattery()
{
  CLog::Log(LOGNOTICE, "%s: Running low battery jobs", __FUNCTION__);

  CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, g_localizeStrings.Get(13050), "");

  CAnnouncementManager::Announce(System, "xbmc", "OnLowBattery");
}
