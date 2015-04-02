/*
 * Copyright (c) 2013-2015 John Connor (BM-NC49AxAjcqVcF5jNPu85Rb8MJ2d9JqZt)
 *
 * This file is part of vanillacoin.
 *
 * vanillacoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef COIN_HASH_SCRYPT_HPP
#define COIN_HASH_SCRYPT_HPP

#include <cstdint>

#include <coin/block.hpp>

namespace coin {

    /**
     * scrypt_buffer_alloc
     */
    void * scrypt_buffer_alloc();

    /**
     * scanhash_scrypt
     */
    std::uint32_t scanhash_scrypt(
        block::header_t * pdata, void * scratchbuf,
        std::uint32_t max_nonce, std::uint32_t & hash_count,
        void * result, block::header_t * res_header
    );

    /**
     * hash_scrypt
     */
    void hash_scrypt(
        const void * input, std::size_t inputlen,
        std::uint32_t * res, void * scratchpad
    );
    
} // namespace coin

#endif // COIN_HASH_SCRYPT_HPP
