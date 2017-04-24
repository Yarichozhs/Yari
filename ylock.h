/*
 *  Yari - In memory Key Value Store 
 *  Copyright (C) 2017  Yari 
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _YLOCK_H

#define _YLOCK_H

/**
 * @brief Lock object. Can be embeed in any object. Need to initialize 
 *        through ylock_init. Contains lock value, owner, acquire and
 *        release locations. 
 */
struct ylock_t
{
  int    val;
  int    owner;
  char  *loc;
  char  *rloc;
};
typedef struct ylock_t ylock_t;

/**
 * @brief Lock state values.
 */
#define YLOCK_NONE   0
#define YLOCK_SHARED 1
#define YLOCK_EXCL   2

/**
 * @brief Lock state type.
 */
typedef int ylock_mode_t;

/**
 * Internal macros for converting the source locations to string. 
 */
#define ylock_strx(x) #x
#define ylock_str(x)  ylock_strx(x)
#define ylock_loc     (__FILE__ ":" ylock_str(__LINE__))

/**
 * @brief Initilize lock object. 
 * 
 * @param l - lock object
 * 
 * @return None. 
 */
#define ylock_init(l) ((l)->val = (l)->owner = 0)

/**
 * @brief Acquire lock object. 
 * 
 * @param l    - lock object
 * @param mode - Mode to acquire. 
 * 
 * @return 0 (success always)
 */
#define ylock_acq(l, mode) \
        ((mode) ? ylock_acq_int(l, mode, TRUE, ylock_loc) : 0)

/**
 * @brief Try acquiring lock object. 
 * 
 * @param l    - lock object
 * @param mode - Mode to acquire. 
 * 
 * @return 0 on success, -1 on failure with errno set. If lock is not in 
 *         compatible mode, returns with -1 and errno set to EBUSY.
 */
#define ylock_try(l, mode) \
        ((mode) ? ylock_acq_int(l, mode, FALSE, ylock_loc) : 0)

/**
 * @brief Release lock object. 
 * 
 * @param l    - lock object
 * @param mode - Acquired Mode. 
 * 
 * @return 0 (success always)
 */
#define ylock_rel(l, mode) \
          ylock_rel_int(l, mode, ylock_loc)

/**
 * @brief Acquire lock object Internal version. 
 *        Do not use this directly, use one of the above acquire interfaces. 
 * 
 * @param l    - lock object
 * @param mode - Mode to acquire. 
 * @param wait - Wait till the lock is acquired. 
 * @param str  - Source location 
 * 
 * @return 0 on succes, -1 on failure with errno set. 
 */
int ylock_acq_int(ylock_t *lock, ylock_mode_t mode, int wait, char *str);

/**
 * @brief Release lock object Internal version. 
 *        Do not use this directly, use one of the above acquire interfaces. 
 * 
 * @param l    - lock object
 * @param mode - Mode acquired. 
 * @param str  - Source location 
 * 
 * @return 0 on succes, -1 on failure with errno set. 
 */
int ylock_rel_int(ylock_t *lock, ylock_mode_t mode, char *str);

#endif /* ylock.h */
