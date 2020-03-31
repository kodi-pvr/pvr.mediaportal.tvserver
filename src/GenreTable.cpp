/*
 *  Copyright (C) 2005-2020 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#include <algorithm>
#include "client.h"
#include "GenreTable.h"
#include "tinyxml.h"

using namespace ADDON;
using namespace std;

bool CGenreTable::LoadGenreXML(const std::string &filename)
{
  TiXmlDocument xmlDoc;
  if (!xmlDoc.LoadFile(filename))
  {
    KODI->Log(LOG_ERROR, "Unable to load %s: %s at line %d", filename.c_str(), xmlDoc.ErrorDesc(), xmlDoc.ErrorRow());
    return false;
  }

  KODI->Log(LOG_NOTICE, "Opened %s to read genre string to type/subtype translation table", filename.c_str());

  TiXmlHandle hDoc(&xmlDoc);
  TiXmlElement* pElem;
  TiXmlHandle hRoot(0);
  const char* sGenreType = NULL;
  const char* sGenreSubType = NULL;
  genre_t genre;

  // block: genrestrings
  pElem = hDoc.FirstChildElement("genrestrings").Element();
  // should always have a valid root but handle gracefully if it does
  if (!pElem)
  {
    KODI->Log(LOG_ERROR, "Could not find <genrestrings> element");
    return false;
  }

  //This should hold: pElem->Value() == "genrestrings"

  // save this for later
  hRoot=TiXmlHandle(pElem);

  // iterate through all genre elements
  TiXmlElement* pGenreNode = hRoot.FirstChildElement("genre").Element();
  //This should hold: pGenreNode->Value() == "genre"

  if (!pGenreNode)
  {
    KODI->Log(LOG_ERROR, "Could not find <genre> element");
    return false;
  }

  for (; pGenreNode != NULL; pGenreNode = pGenreNode->NextSiblingElement("genre"))
  {
    const char* sGenreString = pGenreNode->GetText();

    if (sGenreString)
    {
      sGenreType = pGenreNode->Attribute("type");
      sGenreSubType = pGenreNode->Attribute("subtype");

      if ((sGenreType) && (strlen(sGenreType) > 2))
      {
        if(sscanf(sGenreType + 2, "%5x", &genre.type) != 1)
          genre.type = 0;
      }
      else
      {
        genre.type = 0;
      }

      if ((sGenreSubType) && (strlen(sGenreSubType) > 2 ))
      {
        if(sscanf(sGenreSubType + 2, "%5x", &genre.subtype) != 1)
          genre.subtype = 0;
      }
      else
      {
        genre.subtype = 0;
      }

      if (genre.type > 0)
      {
        KODI->Log(LOG_DEBUG, "Genre '%s' => 0x%x, 0x%x", sGenreString, genre.type, genre.subtype);
        m_genremap.insert(std::pair<std::string, genre_t>(sGenreString, genre));
      }
    }
  }

  return true;
}

void CGenreTable::GenreToTypes(string& strGenre, int& genreType, int& genreSubType)
{
  // The xmltv plugin from the MediaPortal TV Server can return genre
  // strings in local language (depending on the external TV guide source).
  // The only way to solve this at the XMBC side is to transfer the
  // genre string to XBMC or to let this plugin (or the TVServerKodi
  // plugin) translate it into XBMC compatible (numbered) genre types
  string m_genre = strGenre;

  if(!m_genremap.empty() && !m_genre.empty())
  {
    GenreMap::iterator it;

    std::transform(m_genre.begin(), m_genre.end(), m_genre.begin(), ::tolower);

    it = m_genremap.find(m_genre);
    if (it != m_genremap.end())
    {
      genreType = it->second.type;
      genreSubType = it->second.subtype;
    }
    else
    {
      KODI->Log(LOG_DEBUG, "EPG: No mapping of '%s' to genre type/subtype found.", strGenre.c_str());
      genreType     = EPG_GENRE_USE_STRING;
      genreSubType  = 0;
    }
  }
  else
  {
    genreType = 0;
    genreSubType = 0;
  }
}
