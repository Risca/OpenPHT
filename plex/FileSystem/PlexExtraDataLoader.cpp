#include "PlexExtraDataLoader.h"
#include "PlexDirectory.h"
#include "URL.h"
#include "PlexJobs.h"
#include <stdlib.h>
#include "boost/lexical_cast.hpp"
#include "GUIWindowManager.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
CPlexExtraDataLoader::CPlexExtraDataLoader()
{
  m_items = CFileItemListPtr(new CFileItemList());
  m_type = NONE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CPlexExtraDataLoader::loadDataForItem(CFileItemPtr pItem, ExtraDataType type)
{
  if (pItem->GetPath() != m_path)
  {
    m_path = pItem->GetPath();
    m_type = type;
    CURL url(m_path);

    PlexUtils::AppendPathToURL(url, "extras");

    url.SetOptions("");
    url.SetOption("extratype", boost::lexical_cast<std::string>((int)type));

    CJobManager::GetInstance().AddJob(new CPlexDirectoryFetchJob(url), this);
  }
  else
  {
    CGUIMessage msg(GUI_MSG_PLEX_EXTRA_DATA_LOADED, PLEX_EXTRADATA_LOADER, 0, 0, 0);
    g_windowManager.SendThreadMessage(msg);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void CPlexExtraDataLoader::OnJobComplete(unsigned int jobID, bool success, CJob* job)
{
  // grab the job
  CPlexDirectoryFetchJob* fjob = static_cast<CPlexDirectoryFetchJob*>(job);
  if (!fjob)
    return;

  if (success)
  {
    // store the job list
    m_items->Clear();
    if (fjob->m_items.Size())
      m_items->Copy(fjob->m_items);

    // send the dataloaded event to listeners
    CLog::Log(LOGDEBUG, "CPlexExtraInfoLoader : job %d succeeded for %s, (%d extra found)", jobID,
              m_path.c_str(), fjob->m_items.Size());
    CGUIMessage msg(GUI_MSG_PLEX_EXTRA_DATA_LOADED, PLEX_EXTRADATA_LOADER, 0, 0, 0);
    g_windowManager.SendThreadMessage(msg);
  }
  else
  {
    CLog::Log(LOGERROR, "CPlexExtraInfoLoader : job %d failed for %s", jobID, m_path.c_str());
  }
}