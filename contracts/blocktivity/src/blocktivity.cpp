#include "blocktivity.hpp"

void blocktivity::push( const uint64_t block_num, const uint64_t transactions, const uint64_t actions )
{
    require_auth( get_self() );

    add_hour( block_num, transactions, actions );
    calculate_periods( block_num );
}

void blocktivity::add_hour( const uint64_t block_num, const uint64_t transactions, const uint64_t actions )
{
    check( block_num % ONE_HOUR == 0, "[block_num] must be a modulo of " + to_string(ONE_HOUR));
    check( _periods.find( block_num * -1 ) == _periods.end(), "[block_num] already exists" );

    _periods.emplace( get_self(), [&]( auto& row ) {
        row.block_num = block_num;
        row.transactions = transactions;
        row.actions = actions;
    });
}

void blocktivity::calculate_periods( const uint64_t block_num )
{
    // sum stats
    auto sum = _sum.get_or_default();
    auto average = _average.get_or_default();
    auto record = _record.get_or_default();
    auto previous_record = _record.get_or_default();

    // last updated time
    sum.timestamp = current_time_point();
    average.timestamp = current_time_point();

    // counters
    uint64_t actions = 0;
    int count = 0;

    // iterate over each 1 hour period
    auto itr = _periods.begin();
    while ( itr != _periods.end() ) {
        // sum actions
        actions += itr->actions;
        count += 1;

        // update hour/day/week stats
        if (count == 1) {
            sum.hour = actions;

            // calculate record
            if (actions > record.hour) record.hour = actions;
        }
        // 1 day
        if (count == 24) {
            sum.day = actions;

            // calculate record
            if (actions > record.day) record.day = actions;
        }
        // 7 days
        if (count == 168) {
            sum.week = actions;
            average.hour = actions / 168;
            average.day = actions / 7;

            // calculate record
            if (actions > record.week) record.week = actions;
        }

        // erase any hour periods that exceed 1 week
        if ( count > 168 ) itr = _periods.erase( itr );
        if ( itr != _periods.end()) itr++;
    }
    // current block_num must be within 168 periods
    check( _periods.find( block_num * -1 ) != _periods.end(), "[block_num] is older then 168 periods" );

    // save stats
    _sum.set( sum, get_self() );
    _average.set( average, get_self() );

    // only save if record has been modified
    if (previous_record.hour != record.hour || previous_record.day != record.day || previous_record.week != record.week) {
        record.timestamp = current_time_point();
        _record.set( record, get_self() );
    }
}

void blocktivity::clean( const eosio::name table, const std::optional<eosio::name> scope )
{
    require_auth( get_self() );

    // periods
    if (table == "periods"_n) {
        auto periods_itr = _periods.begin();
        while ( periods_itr != _periods.end() ) {
            periods_itr = _periods.erase(periods_itr);
        }
    }
    else if (table == "sum"_n) _sum.remove();
}
