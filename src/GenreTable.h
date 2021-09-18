/*
 *  Copyright (C) 2005-2021 Team Kodi (https://kodi.tv)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSE.md for more information.
 */

#pragma once

#include <map>
#include <string>

typedef struct genre {
  int type;
  int subtype;
} genre_t;

typedef std::map<std::string, genre_t> GenreMap;

class CGenreTable
{
public:
  CGenreTable(const std::string &filename) { LoadGenreXML(filename); };
  bool LoadGenreXML(const std::string &filename);

  /**
   * \brief Convert a genre string into a type/subtype combination using the data in the GenreMap
   * \param strGenre (in)
   * \param genreType (out)
   * \param genreSubType (out)
   */
  void GenreToTypes(std::string& strGenre, int& genreType, int& genreSubType);
private:
  GenreMap m_genremap;
};
