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

#include <coin/block.hpp>
#include <coin/block_index.hpp>
#include <coin/globals.hpp>
#include <coin/hash.hpp>
#include <coin/point_out.hpp>
#include <coin/stack_impl.hpp>
#include <coin/time.hpp>
#include <coin/transaction_index.hpp>
#include <coin/transaction_pool.hpp>
#include <coin/transaction_position.hpp>
#include <coin/utility.hpp>

using namespace coin;

utility::disk_info_t utility::disk_info(const std::string & path)
{
    disk_info_t ret = { 0, 0, 0 };
#if (defined _MSC_VER)
    ULARGE_INTEGER avail, total, free;
    
    std::wstring wpath(path.begin(), path.end());
    
    if (
        ::GetDiskFreeSpaceExW(wpath.c_str(), &avail, &total, &free) != 0
        )
    {
        ret.capacity =
            (static_cast<std::uintmax_t> (total.HighPart) << 32) +
            total.LowPart
        ;
        ret.free =
            (static_cast<std::uintmax_t> (free.HighPart) << 32) +
            free.LowPart
        ;
        ret.available =
            (static_cast<std::uintmax_t> (avail.HighPart) << 32) +
            avail.LowPart
        ;
    }
#else
    struct statvfs vfs;

    if (statvfs(path.c_str(), &vfs) == 0)
    {
        ret.capacity =
            static_cast<std::uintmax_t>(vfs.f_blocks) * vfs.f_frsize
        ;
        ret.free =
            static_cast<std::uintmax_t>(vfs.f_bfree) * vfs.f_frsize
        ;
        ret.available =
            static_cast<std::uintmax_t>(vfs.f_bavail) * vfs.f_frsize
        ;
    }
#endif // _MSC_VER
    return ret;
}

std::vector<std::uint8_t> utility::from_hex(const std::string & val)
{
    static const std::int8_t g_hex_digit[256] =
    {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        0,1,2,3,4,5,6,7,8,9,-1,-1,-1,-1,-1,-1,
        -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,0xa,0xb,0xc,0xd,0xe,0xf,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

    const char * ptr = val.c_str();
    
    std::vector<std::uint8_t> vch;
    
    for (;;)
    {
        while (std::isspace(*ptr))
        {
            ptr++;
        }
        
        std::int8_t c = g_hex_digit[(std::uint8_t)*ptr++];
        
        if (c == (std::int8_t)-1)
        {
            break;
        }
        
        std::uint8_t n = (c << 4);
        
        c = g_hex_digit[(std::uint8_t)*ptr++];
        
        if (c == (std::int8_t)-1)
        {
            break;
        }
        
        n |= c;
    
        vch.push_back(n);
    }
    
    return vch;
}

bool utility::is_initial_block_download()
{
    if (
        stack_impl::get_block_index_best() == 0 ||
        globals::instance().best_block_height() <
        checkpoints::instance().get_total_blocks_estimate()
        )
    {
        return true;
    }
    
    static std::time_t g_last_update;
    static std::shared_ptr<block_index> g_index_last_best;

    if (stack_impl::get_block_index_best() != g_index_last_best)
    {
        g_index_last_best = stack_impl::get_block_index_best();
        g_last_update = std::time(0);
    }
    
    return
        std::time(0) - g_last_update < 10 &&
        stack_impl::get_block_index_best()->time() <
        std::time(0) - 24 * 60 * 60
    ;
}

bool utility::is_chain_file(const std::string & file_name)
{
    return file_name == "blkindex.dat";
}

sha256 utility::get_orphan_root(const std::shared_ptr<block> & blk)
{
    auto * ptr = blk.get();

    /**
     * Work back to the first block in the orphan chain.
     */
    while (
        globals::instance().orphan_blocks().count(
        ptr->header().hash_previous_block) > 0
        )
    {
        ptr = globals::instance().orphan_blocks()[
            ptr->header().hash_previous_block
        ].get();
    }
    
    return ptr->get_hash();
}

sha256 utility::wanted_by_orphan(const std::shared_ptr<block> & blk)
{
    auto * ptr = blk.get();
    
    /**
     * Work back to the first block in the orphan chain.
     */
    while (
        globals::instance().orphan_blocks().count(
        ptr->header().hash_previous_block) > 0
        )
    {
        ptr = globals::instance().orphan_blocks()[
            ptr->header().hash_previous_block
        ].get();
    }
    
    return ptr->header().hash_previous_block;
}

bool utility::add_orphan_tx(const data_buffer & buffer)
{
    /**
     * Copy the buffer.
     */
    auto buffer_copy = std::make_shared<data_buffer> (
        buffer.data(), buffer.size()
    );
    
    /**
     * Allocate the transaction.
     */
    transaction tx;
    
    /**
     * Decode the transaction from the buffer.
     */
    tx.decode(*buffer_copy);
    
    /**
     * Rewind the buffer.
     */
    buffer_copy->rewind();
    
    /**
     * Get the hash of the transaction.
     */
    auto hash_tx = tx.get_hash();
    
    if (globals::instance().orphan_transactions().count(hash_tx) > 0)
    {
        return false;
    }

    /**
     * Ignore big transactions, to avoid a send-big-orphans memory
     * exhaustion attack. If a peer has a legitimate large transaction
     * with a missing parent then we assume it will rebroadcast it later,
     * after the parent transaction(s) have been mined or received.
     * 10,000 orphans, each of which is at most 5,000 bytes big is at most 
     * 500 megabytes of orphans.
     */
    if (buffer_copy->size() > 5000)
    {
        log_debug(
            "Utility, add orphan tx ignoring large orphan tx size = " <<
            buffer_copy->size() << ", hash = " <<
            hash_tx.to_string().substr(0, 10)  << "."
        );

        return false;
    }

    globals::instance().orphan_transactions()[hash_tx] = buffer_copy;
    
    for (auto & i : tx.transactions_in())
    {
        globals::instance().orphan_transactions_by_previous()[
            i.previous_out().get_hash()].insert(
            std::make_pair(hash_tx, buffer_copy)
        );
    }

    log_debug(
        "Utility, add orphan tx stored orphan tx " <<
        hash_tx.to_string().substr(0, 10) << ", orphans = " <<
        globals::instance().orphan_transactions().size() << "."
    );
    
    return true;
}

void utility::erase_orphan_tx(const sha256 & hash_tx)
{
    if (globals::instance().orphan_transactions().count(hash_tx) > 0)
    {
        const auto & buffer =
            globals::instance().orphan_transactions()[hash_tx]
        ;
        
        transaction tx;
        
        tx.decode(*buffer);
        
        buffer->rewind();
        
        for (auto & i : tx.transactions_in())
        {
            globals::instance().orphan_transactions_by_previous()[
                i.previous_out().get_hash()
            ].erase(hash_tx);
            
            if (
                globals::instance().orphan_transactions_by_previous()[
                i.previous_out().get_hash()].size() == 0
                )
            {
                globals::instance().orphan_transactions_by_previous().erase(
                    i.previous_out().get_hash()
                );
            }
        }

        globals::instance().orphan_transactions().erase(hash_tx);
    }
}

std::uint32_t utility::limit_orphan_tx_size(const std::uint32_t & max_orphans)
{
    std::uint32_t evicted = 0;
    
    while (globals::instance().orphan_transactions().size() > max_orphans)
    {
        /**
         * Evict a random orphan.
         */
        auto hash_random = hash::sha256_random();
        
        auto it =
            globals::instance().orphan_transactions().lower_bound(
            hash_random)
        ;
        
        if (it == globals::instance().orphan_transactions().end())
        {
            it = globals::instance().orphan_transactions().begin();
        }
        
        erase_orphan_tx(it->first);
        
        ++evicted;
    }
    
    return evicted;
}

const std::shared_ptr<block_index> utility::get_last_block_index(
    const std::shared_ptr<block_index> & index, const bool & is_pos
    )
{
    auto tmp = index;
    
    while (
        tmp && tmp->block_index_previous() &&
        (tmp->is_proof_of_stake() != is_pos)
        )
    {
        tmp = tmp->block_index_previous();
    }
    
    return tmp;
}

std::shared_ptr<block_index> utility::find_block_index_by_height(
    const std::uint32_t & height
    )
{
    std::shared_ptr<block_index> ret;
    
    if (height < stack_impl::get_block_index_best()->height() / 2)
    {
        ret = stack_impl::get_block_index_genesis();
    }
    else
    {
        ret = stack_impl::get_block_index_best();
    }
    
    if (
        globals::instance().block_index_fbbh_last() &&
        std::abs(static_cast<std::int64_t> (height - ret->height())) >
        std::abs(static_cast<std::int64_t> (height - globals::instance(
        ).block_index_fbbh_last()->height()))
        )
    {
        ret = globals::instance().block_index_fbbh_last();
    }
    
    while (ret->height() > height)
    {
        ret = ret->block_index_previous();
    }
    
    while (ret->height() < height)
    {
        ret = ret->block_index_next();
    }
    
    globals::instance().set_block_index_fbbh_last(ret);
    
    return ret;
}

std::uint32_t utility::compute_max_bits(
    big_number target_limit, std::uint32_t base, std::int64_t time
    )
{
    big_number ret;
    
    ret.set_compact(base);
    
    ret *= 2;
    
    while (time > 0 && ret < target_limit)
    {
        /**
         * Maximum 200% adjustment per day.
         */
        ret *= 2;
        
        time -= 24 * 60 * 60;
    }
    
    if (ret > target_limit)
    {
        ret = target_limit;
    }
    
    return ret.get_compact();
}

std::uint32_t utility::compute_min_work(
    std::uint32_t base, std::int64_t time
    )
{
    return compute_max_bits(
        constants::proof_of_work_limit, base, time
    );
}

std::uint32_t utility::compute_min_stake(
    std::uint32_t base, std::int64_t time, std::uint32_t time_block
    )
{
    return compute_max_bits(constants::proof_of_stake_limit, base, time);
}

std::uint32_t utility::get_next_target_required(
    const std::shared_ptr<block_index> & index_last, const bool & is_pos
    )
{
    big_number ret;
    
    /**
     * The next block height.
     */
    auto height = index_last->height() + 1;
    
    (void)height;

    /**
     * Get the target limit.
     */
    big_number target_limit =
        is_pos ? constants::proof_of_stake_limit :
        constants::proof_of_work_limit
    ;
    
    /*
     * Proof-of-Stake blocks have their own target limit.
     */
    if (is_pos)
    {
        target_limit = constants::proof_of_stake_limit;
    }

    /**
     * The genesis block.
     */
    if (index_last == 0)
    {
        return target_limit.get_compact();
    }
    
    auto index_previous = get_last_block_index(index_last, is_pos);

    /**
     * The first block.
     */
    if (index_previous->block_index_previous() == 0)
    {
        /**
         * Set the difficulty to 0.00388934.
         */
        ret.set_compact(503382300);
        
        return ret.get_compact();
    }
    
    auto index_previous_previous =
        get_last_block_index(index_previous->block_index_previous(), is_pos
    );

    /**
     * The second block.
     */
    if (index_previous_previous->block_index_previous() == 0)
    {
        /**
         * Set the difficulty to 0.00388934.
         */
        ret.set_compact(503382300);
        
        return ret.get_compact();
    }

    /**
     * 20 minutes.
     */
    static const std::int64_t target_timespan = 20 * 60;
    
    /**
     * Spacing
     */
    std::int64_t target_spacing = constants::work_and_stake_target_spacing;

    /**
     * Set the bits.
     */
    ret.set_compact(index_previous->bits());
    
    /**
     * Calculate the actual spacing.
     */
    std::int64_t
        actual_spacing = index_previous->time() -
        index_previous_previous->time()
    ;

    if (actual_spacing < 0)
    {
        actual_spacing = constants::work_and_stake_target_spacing;
    }
    
    /**
     * Retarget
     */
    ret *=
        (((target_timespan / target_spacing) - 1) *
        target_spacing + actual_spacing + actual_spacing)
    ;
    ret /=
        (((target_timespan / target_spacing) + 1) * target_spacing)
    ;

    if (ret > target_limit)
    {
        ret = target_limit;
    }

    return ret.get_compact();
}

bool utility::get_transaction(
    const sha256 & hash_tx, transaction & tx, sha256 & hash_block_out
    )
{
    if (transaction_pool::instance().exists(hash_tx))
    {
        tx = transaction_pool::instance().lookup(hash_tx);
        
        return true;
    }
    
    db_tx tx_db("r");
    
    transaction_index index;
    
    if (tx.read_from_disk(tx_db, point_out(hash_tx, 0), index))
    {
        block blk;
        
        if (
            blk.read_from_disk(index.get_transaction_position().file_index(),
            index.get_transaction_position().block_position(), false)
            )
        {
            hash_block_out = blk.get_hash();
        }
        return true;
    }
    
    return false;
}
