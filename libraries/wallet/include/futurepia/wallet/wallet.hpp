#pragma once

#include <futurepia/app/api.hpp>
#include <futurepia/private_message/private_message_plugin.hpp>
#include <futurepia/token/token_api.hpp>
#include <futurepia/app/futurepia_api_objects.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <fc/real128.hpp>
#include <fc/crypto/base58.hpp>

using namespace futurepia::app;
using namespace futurepia::chain;
using namespace graphene::utilities;
using namespace std;

namespace futurepia { namespace wallet {

using namespace futurepia::private_message;
using namespace futurepia::token;

typedef uint16_t transaction_handle_type;

struct memo_data {

   static optional<memo_data> from_string( string str ) {
      try {
         if( str.size() > sizeof(memo_data) && str[0] == '#') {
            auto data = fc::from_base58( str.substr(1) );
            auto m  = fc::raw::unpack<memo_data>( data );
            FC_ASSERT( string(m) == str );
            return m;
         }
      } catch ( ... ) {}
      return optional<memo_data>();
   }

   public_key_type from;
   public_key_type to;
   uint64_t        nonce = 0;
   uint32_t        check = 0;
   vector<char>    encrypted;

   operator string()const {
      auto data = fc::raw::pack(*this);
      auto base58 = fc::to_base58( data );
      return '#'+base58;
   }
};



struct brain_key_info
{
   string               brain_priv_key;
   public_key_type      pub_key;
   string               wif_priv_key;
};

struct wallet_data
{
   vector<char>              cipher_keys; /** encrypted keys */

   string                    ws_server = "ws://localhost:5020";
   string                    ws_user;
   string                    ws_password;
};

enum authority_type
{
   owner,
   active,
   posting
};

namespace detail {
class wallet_api_impl;
}

/**
 * This wallet assumes it is connected to the database server with a high-bandwidth, low-latency connection and
 * performs minimal caching. This API could be provided locally to be used by a web interface.
 */
class wallet_api
{
   public:
      wallet_api( const wallet_data& initial_data, fc::api<login_api> rapi );
      virtual ~wallet_api();

      bool copy_wallet_file( string destination_filename );


      /** Returns a list of all commands supported by the wallet API.
       *
       * This lists each command, along with its arguments and return types.
       * For more detailed help on a single command, use \c get_help()
       *
       * @returns a multi-line string suitable for displaying on a terminal
       */
      string                              help()const;

      /**
       * Returns info about the current state of the blockchain
       */
      variant                             info();

      /** Returns info such as client version, git version of graphene/fc, version of boost, openssl.
       * @returns compile time info and client and dependencies versions
       */
      variant_object                      about() const;

      /** Returns the information about a block
       *
       * @param num Block num
       *
       * @returns Public block data on the blockchain
       */
      optional<signed_block_api_obj>    get_block( uint32_t num );

      /** Returns sequence of operations included/generated in a specified block
       *
       * @param block_num Block height of specified block
       * @param only_virtual Whether to only return virtual operations
       */
      vector<applied_operation>           get_ops_in_block( uint32_t block_num, bool only_virtual = true );

      /** Return the current price feed history
       *
       * @returns Price feed history data on the blockchain
       */
      feed_history_api_obj                 get_feed_history()const;

      /**
       * Returns the list of bobservers producing blocks in the current round (21 Blocks)
       */
      vector<account_name_type>                      get_active_bobservers()const;

      /**
       * Returns the queue of pow miners waiting to produce blocks.
       */
      vector<account_name_type>                      get_miner_queue()const;

      /**
       * Returns the state info associated with the URL
       */
      app::state                          get_state( string url );

      /**
       *  Gets the account information for all accounts for which this wallet has a private key
       */
      vector<account_api_obj>              list_my_accounts();

      /** Lists all accounts registered in the blockchain.
       * This returns a list of all account names and their account ids, sorted by account name.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all accounts,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last account name returned as the \c lowerbound for the next \c list_accounts() call.
       *
       * @param lowerbound the name of the first account to return.  If the named account does not exist,
       *                   the list will start at the account that comes after \c lowerbound
       * @param limit the maximum number of accounts to return (max: 1000)
       * @returns a list of accounts mapping account names to account ids
       */
      set<string>  list_accounts(const string& lowerbound, uint32_t limit);

      /** Returns the block chain's rapidly-changing properties.
       * The returned object contains information that changes every block interval
       * such as the head block number, the next bobserver, etc.
       * @see \c get_global_properties() for less-frequently changing properties
       * @returns the dynamic global properties
       */
      dynamic_global_property_api_obj    get_dynamic_global_properties() const;

      /** Returns information about the given account.
       *
       * @param account_name the name of the account to provide information about
       * @returns the public account data stored in the blockchain
       */
      account_api_obj                     get_account( string account_name ) const;

      /** Returns the current wallet filename.
       *
       * This is the filename that will be used when automatically saving the wallet.
       *
       * @see set_wallet_filename()
       * @return the wallet filename
       */
      string                            get_wallet_filename() const;

      /**
       * Get the WIF private key corresponding to a public key.  The
       * private key must already be in the wallet.
       */
      string                            get_private_key( public_key_type pubkey )const;

      /**
       *  @param role - active | owner | posting | memo
       */
      pair<public_key_type,string>  get_private_key_from_password( string account, string role, string password )const;


      /**
       * Returns transaction by ID.
       */
      annotated_signed_transaction      get_transaction( transaction_id_type trx_id )const;

      /** Checks whether the wallet has just been created and has not yet had a password set.
       *
       * Calling \c set_password will transition the wallet to the locked state.
       * @return true if the wallet is new
       * @ingroup Wallet Management
       */
      bool    is_new()const;

      /** Checks whether the wallet is locked (is unable to use its private keys).
       *
       * This state can be changed by calling \c lock() or \c unlock().
       * @return true if the wallet is locked
       * @ingroup Wallet Management
       */
      bool    is_locked()const;

      /** Locks the wallet immediately.
       * @ingroup Wallet Management
       */
      void    lock();

      /** Unlocks the wallet.
       *
       * The wallet remain unlocked until the \c lock is called
       * or the program exits.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      void    unlock(string password);

      /** Sets a new password on the wallet.
       *
       * The wallet must be either 'new' or 'unlocked' to
       * execute this command.
       * @ingroup Wallet Management
       */
      void    set_password(string password);

      /** Dumps all private keys owned by the wallet.
       *
       * The keys are printed in WIF format.  You can import these keys into another wallet
       * using \c import_key()
       * @returns a map containing the private keys, indexed by their public key
       */
      map<public_key_type, string> list_keys();

      /** Returns detailed help on a single API command.
       * @param method the name of the API command you want help with
       * @returns a multi-line string suitable for displaying on a terminal
       */
      string  gethelp(const string& method)const;

      /** Loads a specified Graphene wallet.
       *
       * The current wallet is closed before the new wallet is loaded.
       *
       * @warning This does not change the filename that will be used for future
       * wallet writes, so this may cause you to overwrite your original
       * wallet unless you also call \c set_wallet_filename()
       *
       * @param wallet_filename the filename of the wallet JSON file to load.
       *                        If \c wallet_filename is empty, it reloads the
       *                        existing wallet file
       * @returns true if the specified wallet is loaded
       */
      bool    load_wallet_file(string wallet_filename = "");

      /** Saves the current wallet to the given filename.
       *
       * @warning This does not change the wallet filename that will be used for future
       * writes, so think of this function as 'Save a Copy As...' instead of
       * 'Save As...'.  Use \c set_wallet_filename() to make the filename
       * persist.
       * @param wallet_filename the filename of the new wallet JSON file to create
       *                        or overwrite.  If \c wallet_filename is empty,
       *                        save to the current filename.
       */
      void    save_wallet_file(string wallet_filename = "");

      /** Sets the wallet filename used for future writes.
       *
       * This does not trigger a save, it only changes the default filename
       * that will be used the next time a save is triggered.
       *
       * @param wallet_filename the new filename to use for future saves
       */
      void    set_wallet_filename(string wallet_filename);

      /** Suggests a safe brain key to use for creating your account.
       * \c create_account_with_brain_key() requires you to specify a 'brain key',
       * a long passphrase that provides enough entropy to generate cyrptographic
       * keys.  This function will suggest a suitably random string that should
       * be easy to write down (and, with effort, memorize).
       * @returns a suggested brain_key
       */
      brain_key_info suggest_brain_key()const;

      /** Converts a signed_transaction in JSON form to its binary representation.
       *
       * TODO: I don't see a broadcast_transaction() function, do we need one?
       *
       * @param tx the transaction to serialize
       * @returns the binary form of the transaction.  It will not be hex encoded,
       *          this returns a raw string that may have null characters embedded
       *          in it
       */
      string serialize_transaction(signed_transaction tx) const;

      /** Imports a WIF Private Key into the wallet to be used to sign transactions by an account.
       *
       * example: import_key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
       *
       * @param wif_key the WIF Private Key to import
       */
      bool import_key( string wif_key );

      /** Transforms a brain key to reduce the chance of errors when re-entering the key from memory.
       *
       * This takes a user-supplied brain key and normalizes it into the form used
       * for generating private keys.  In particular, this upper-cases all ASCII characters
       * and collapses multiple spaces into one.
       * @param s the brain key as supplied by the user
       * @returns the brain key in its normalized form
       */
      string normalize_brain_key(string s) const;

      /**
       *  This method will genrate new owner, active, and memo keys for the new account which
       *  will be controlable by this wallet. There is a fee associated with account creation
       *  that is paid by the creator. The current account creation fee can be found with the
       *  'info' wallet command.
       *
       *  @param creator The account creating the new account
       *  @param new_account_name The name of the new account
       *  @param json_meta JSON Metadata associated with the new account
       *  @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction create_account( string creator, string new_account_name, string json_meta, bool broadcast );

      /**
       * This method is used by faucets to create new accounts for other users which must
       * provide their desired keys. The resulting account may not be controllable by this
       * wallet. There is a fee associated with account creation that is paid by the creator.
       * The current account creation fee can be found with the 'info' wallet command.
       *
       * @param creator The account creating the new account
       * @param newname The name of the new account
       * @param json_meta JSON Metadata associated with the new account
       * @param owner public owner key of the new account
       * @param active public active key of the new account
       * @param posting public posting key of the new account
       * @param memo public memo key of the new account
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction create_account_with_keys( string creator,
                                            string newname,
                                            string json_meta,
                                            public_key_type owner,
                                            public_key_type active,
                                            public_key_type posting,
                                            public_key_type memo,
                                            bool broadcast )const;

      /**
       *  This method will genrate new owner, active, and memo keys for the new account which
       *  will be controlable by this wallet. There is a fee associated with account creation
       *  that is paid by the creator. The current account creation fee can be found with the
       *  'info' wallet command.
       *
       *  These accounts are created with combination of FPC and delegated SP
       *
       *  @param creator The account creating the new account
       *  @param futurepia_fee The amount of the fee to be paid with FPC
       *  @param new_account_name The name of the new account
       *  @param json_meta JSON Metadata associated with the new account
       *  @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction create_account_delegated( string creator, asset futurepia_fee, string new_account_name, string json_meta, bool broadcast );

      /**
       * This method is used by faucets to create new accounts for other users which must
       * provide their desired keys. The resulting account may not be controllable by this
       * wallet. There is a fee associated with account creation that is paid by the creator.
       * The current account creation fee can be found with the 'info' wallet command.
       *
       * These accounts are created with combination of FPC and delegated SP
       *
       * @param creator The account creating the new account
       * @param futurepia_fee The amount of the fee to be paid with FPC
       * @param newname The name of the new account
       * @param json_meta JSON Metadata associated with the new account
       * @param owner public owner key of the new account
       * @param active public active key of the new account
       * @param posting public posting key of the new account
       * @param memo public memo key of the new account
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction create_account_with_keys_delegated( string creator,
                                            asset futurepia_fee,
                                            string newname,
                                            string json_meta,
                                            public_key_type owner,
                                            public_key_type active,
                                            public_key_type posting,
                                            public_key_type memo,
                                            bool broadcast )const;

      /**
       * This method updates the keys of an existing account.
       *
       * @param accountname The name of the account
       * @param json_meta New JSON Metadata to be associated with the account
       * @param owner New public owner key for the account
       * @param active New public active key for the account
       * @param posting New public posting key for the account
       * @param memo New public memo key for the account
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction update_account( string accountname,
                                         string json_meta,
                                         public_key_type owner,
                                         public_key_type active,
                                         public_key_type posting,
                                         public_key_type memo,
                                         bool broadcast )const;

      /**
       * This method updates the key of an authority for an exisiting account.
       * Warning: You can create impossible authorities using this method. The method
       * will fail if you create an impossible owner authority, but will allow impossible
       * active and posting authorities.
       *
       * @param account_name The name of the account whose authority you wish to update
       * @param type The authority type. e.g. owner, active, or posting
       * @param key The public key to add to the authority
       * @param weight The weight the key should have in the authority. A weight of 0 indicates the removal of the key.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      annotated_signed_transaction update_account_auth_key( string account_name, authority_type type, public_key_type key, weight_type weight, bool broadcast );

      /**
       * This method updates the account of an authority for an exisiting account.
       * Warning: You can create impossible authorities using this method. The method
       * will fail if you create an impossible owner authority, but will allow impossible
       * active and posting authorities.
       *
       * @param account_name The name of the account whose authority you wish to update
       * @param type The authority type. e.g. owner, active, or posting
       * @param auth_account The account to add the the authority
       * @param weight The weight the account should have in the authority. A weight of 0 indicates the removal of the account.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      annotated_signed_transaction update_account_auth_account( string account_name, authority_type type, string auth_account, weight_type weight, bool broadcast );

      /**
       * This method updates the weight threshold of an authority for an account.
       * Warning: You can create impossible authorities using this method as well
       * as implicitly met authorities. The method will fail if you create an implicitly
       * true authority and if you create an impossible owner authoroty, but will allow
       * impossible active and posting authorities.
       *
       * @param account_name The name of the account whose authority you wish to update
       * @param type The authority type. e.g. owner, active, or posting
       * @param threshold The weight threshold required for the authority to be met
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction update_account_auth_threshold( string account_name, authority_type type, uint32_t threshold, bool broadcast );

      /**
       * This method updates the account JSON metadata
       *
       * @param account_name The name of the account you wish to update
       * @param json_meta The new JSON metadata for the account. This overrides existing metadata
       * @param broadcast ture if you wish to broadcast the transaction
       */
      annotated_signed_transaction update_account_meta( string account_name, string json_meta, bool broadcast );

      /**
       * This method updates the memo key of an account
       *
       * @param account_name The name of the account you wish to update
       * @param key The new memo public key
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction update_account_memo_key( string account_name, public_key_type key, bool broadcast );


      /**
       *  This method is used to convert a JSON transaction to its transaction ID.
       */
      transaction_id_type get_transaction_id( const signed_transaction& trx )const { return trx.id(); }

      /** Lists all bobservers registered in the blockchain.
       * This returns a list of all account names that own bobservers, and the associated bobserver id,
       * sorted by name.  This lists bobservers whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all bobservers,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last bobserver name returned as the \c lowerbound for the next \c list_bobservers() call.
       *
       * @param lowerbound the name of the first bobserver to return.  If the named bobserver does not exist,
       *                   the list will start at the bobserver that comes after \c lowerbound
       * @param limit the maximum number of bobservers to return (max: 1000)
       * @returns a list of bobservers mapping bobserver names to bobserver ids
       */
      set<account_name_type>       list_bobservers(const string& lowerbound, uint32_t limit);

      /** Returns information about the given bobserver.
       * @param owner_account the name or id of the bobserver account owner, or the id of the bobserver
       * @returns the information about the bobserver stored in the block chain
       */
      optional< bobserver_api_obj > get_bobserver(string owner_account);

      /** Returns conversion requests by an account
       *
       * @param owner Account name of the account owning the requests
       *
       * @returns All pending conversion requests by account
       */
      vector<convert_request_api_obj> get_conversion_requests( string owner );


      /**
       * Transfer funds from one account to another. FPC and FPCH can be transferred.
       *
       * @param from The account the funds are coming from
       * @param to The account the funds are going to
       * @param amount The funds being transferred. i.e. "100.000 FPC"
       * @param memo A memo for the transactionm, encrypted with the to account's public memo key
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction transfer(string from, string to, asset amount, string memo, bool broadcast = false);

      /**
       * Transfer funds from one account to another using escrow. FPC and FPCH can be transferred.
       *
       * @param from The account the funds are coming from
       * @param to The account the funds are going to
       * @param agent The account acting as the agent in case of dispute
       * @param escrow_id A unique id for the escrow transfer. (from, escrow_id) must be a unique pair
       * @param fpch_amount The amount of FPCH to transfer
       * @param futurepia_amount The amount of FPC to transfer
       * @param fee The fee paid to the agent
       * @param ratification_deadline The deadline for 'to' and 'agent' to approve the escrow transfer
       * @param escrow_expiration The expiration of the escrow transfer, after which either party can claim the funds
       * @param json_meta JSON encoded meta data
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction escrow_transfer(
         string from,
         string to,
         string agent,
         uint32_t escrow_id,
         asset fpch_amount,
         asset futurepia_amount,
         asset fee,
         time_point_sec ratification_deadline,
         time_point_sec escrow_expiration,
         string json_meta,
         bool broadcast = false
      );

      /**
       * Approve a proposed escrow transfer. Funds cannot be released until after approval. This is in lieu of requiring
       * multi-sig on escrow_transfer
       *
       * @param from The account that funded the escrow
       * @param to The destination of the escrow
       * @param agent The account acting as the agent in case of dispute
       * @param who The account approving the escrow transfer (either 'to' or 'agent)
       * @param escrow_id A unique id for the escrow transfer
       * @param approve true to approve the escrow transfer, otherwise cancels it and refunds 'from'
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction escrow_approve(
         string from,
         string to,
         string agent,
         string who,
         uint32_t escrow_id,
         bool approve,
         bool broadcast = false
      );

      /**
       * Raise a dispute on the escrow transfer before it expires
       *
       * @param from The account that funded the escrow
       * @param to The destination of the escrow
       * @param agent The account acting as the agent in case of dispute
       * @param who The account raising the dispute (either 'from' or 'to')
       * @param escrow_id A unique id for the escrow transfer
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction escrow_dispute(
         string from,
         string to,
         string agent,
         string who,
         uint32_t escrow_id,
         bool broadcast = false
      );

      /**
       * Release funds help in escrow
       *
       * @param from The account that funded the escrow
       * @param to The account the funds are originally going to
       * @param agent The account acting as the agent in case of dispute
       * @param who The account authorizing the release
       * @param receiver The account that will receive funds being released
       * @param escrow_id A unique id for the escrow transfer
       * @param fpch_amount The amount of FPCH that will be released
       * @param futurepia_amount The amount of FPC that will be released
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction escrow_release(
         string from,
         string to,
         string agent,
         string who,
         string receiver,
         uint32_t escrow_id,
         asset fpch_amount,
         asset futurepia_amount,
         bool broadcast = false
      );


      /**
       *  Transfers into savings happen immediately, transfers from savings take 72 hours
       */
      annotated_signed_transaction transfer_to_savings( string from, string to, asset amount, string memo, bool broadcast = false );

      /**
       * @param request_id - an unique ID assigned by from account, the id is used to cancel the operation and can be reused after the transfer completes
       */
      annotated_signed_transaction transfer_from_savings( string from, uint32_t request_id, string to, asset amount, string memo, bool broadcast = false );

      /**
       *  @param request_id the id used in transfer_from_savings
       *  @param from the account that initiated the transfer
       */
      annotated_signed_transaction cancel_transfer_from_savings( string from, uint32_t request_id, bool broadcast = false );


      /**
       *  This method will convert FPCH to FPC at the current_median_history price one
       *  week from the time it is executed. This method depends upon there being a valid price feed.
       *
       *  @param from The account requesting conversion of its FPCH i.e. "1.000 FPCH"
       *  @param amount The amount of FPCH to convert
       *  @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction convert_fpch( string from, asset amount, bool broadcast = false );

      /**
       * A bobserver can public a price feed for the FPC:FPCH market. The median price feed is used
       * to process conversion requests from FPCH to FPC.
       *
       * @param bobserver The bobserver publishing the price feed
       * @param exchange_rate The desired exchange rate
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction publish_feed(string bobserver, price exchange_rate, bool broadcast );

      /** Signs a transaction.
       *
       * Given a fully-formed transaction that is only lacking signatures, this signs
       * the transaction with the necessary keys and optionally broadcasts the transaction
       * @param tx the unsigned transaction
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      annotated_signed_transaction sign_transaction(signed_transaction tx, bool broadcast = false);

      /** Returns an uninitialized object representing a given blockchain operation.
       *
       * This returns a default-initialized object of the given type; it can be used
       * during early development of the wallet when we don't yet have custom commands for
       * creating all of the operations the blockchain supports.
       *
       * Any operation the blockchain supports can be created using the transaction builder's
       * \c add_operation_to_builder_transaction() , but to do that from the CLI you need to
       * know what the JSON form of the operation looks like.  This will give you a template
       * you can fill in.  It's better than nothing.
       *
       * @param operation_type the type of operation to return, must be one of the
       *                       operations defined in `futurepia/chain/operations.hpp`
       *                       (e.g., "global_parameters_update_operation")
       * @return a default-constructed operation of the given type
       */
      operation get_prototype_operation(string operation_type);

      void network_add_nodes( const vector<string>& nodes );
      vector< variant > network_get_connected_peers();

      /**
       * Gets the current order book for FPC:FPCH
       *
       * @param limit Maximum number of orders to return for bids and asks. Max is 1000.
       */
      order_book  get_order_book( uint32_t limit = 1000 );
      vector<extended_limit_order>  get_open_orders( string accountname );

      /**
       *  Creates a limit order at the price amount_to_sell / min_to_receive and will deduct amount_to_sell from account
       *
       *  @param owner The name of the account creating the order
       *  @param order_id is a unique identifier assigned by the creator of the order, it can be reused after the order has been filled
       *  @param amount_to_sell The amount of either FPCH or FPC you wish to sell
       *  @param min_to_receive The amount of the other asset you will receive at a minimum
       *  @param fill_or_kill true if you want the order to be killed if it cannot immediately be filled
       *  @param expiration the time the order should expire if it has not been filled
       *  @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction create_order( string owner, uint32_t order_id, asset amount_to_sell, asset min_to_receive, bool fill_or_kill, uint32_t expiration, bool broadcast );

      /**
       * Cancel an order created with create_order
       *
       * @param owner The name of the account owning the order to cancel_order
       * @param orderid The unique identifier assigned to the order by its creator
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction cancel_order( string owner, uint32_t orderid, bool broadcast );

      /**
       *  send private message
       *
       *  @param from sender account
       *  @param to receiver account
       *  @param subject subject of message
       *  @param body message body
       *  @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction      send_private_message( string from, string to, string subject, string body, bool broadcast );

      /**
       *  get inbox (received message)
       *
       *  @param account inbox account
       *  @param newest get message before "newest". format is "yyyy-MM-ddThh:mm:ss" 
       *  @param limit the maximum number of message to return (max: 100)
       */
      vector<extended_message_object>   get_inbox( string account, fc::time_point newest, uint32_t limit );

      /**
       *  get outbox (sent message)
       *
       *  @param account outbox account
       *  @param newest get message before "newest". format is "yyyy-MM-ddThh:mm:ss" 
       *  @param limit the maximum number of message to return (max: 100)
       */
      vector<extended_message_object>   get_outbox( string account, fc::time_point newest, uint32_t limit );
      message_body try_decrypt_message( const message_api_obj& mo );

      /**
       * Sets the amount of time in the future until a transaction expires.
       */
      void set_transaction_expiration(uint32_t seconds);

      /**
       * Challenge a user's authority. The challenger pays a fee to the challenged which is depositted as
       * Futurepia Power. Until the challenged proves their active key, all posting rights are revoked.
       *
       * @param challenger The account issuing the challenge
       * @param challenged The account being challenged
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction challenge( string challenger, string challenged, bool broadcast );

      /**
       * Create an account recovery request as a recover account. The syntax for this command contains a serialized authority object
       * so there is an example below on how to pass in the authority.
       *
       * request_account_recovery "your_account" "account_to_recover" {"weight_threshold": 1,"account_auths": [], "key_auths": [["new_public_key",1]]} true
       *
       * @param recovery_account The name of your account
       * @param account_to_recover The name of the account you are trying to recover
       * @param new_authority The new owner authority for the recovered account. This should be given to you by the holder of the compromised or lost account.
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction request_account_recovery( string recovery_account, string account_to_recover, authority new_authority, bool broadcast );

      /**
       * Recover your account using a recovery request created by your recovery account. The syntax for this commain contains a serialized
       * authority object, so there is an example below on how to pass in the authority.
       *
       * recover_account "your_account" {"weight_threshold": 1,"account_auths": [], "key_auths": [["old_public_key",1]]} {"weight_threshold": 1,"account_auths": [], "key_auths": [["new_public_key",1]]} true
       *
       * @param account_to_recover The name of your account
       * @param recent_authority A recent owner authority on your account
       * @param new_authority The new authority that your recovery account used in the account recover request.
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction recover_account( string account_to_recover, authority recent_authority, authority new_authority, bool broadcast );

      /**
       * Change your recovery account after a 30 day delay.
       *
       * @param owner The name of your account
       * @param new_recovery_account The name of the recovery account you wish to have
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction change_recovery_account( string owner, string new_recovery_account, bool broadcast );

      vector< owner_authority_history_api_obj > get_owner_history( string account )const;

      /**
       * Prove an account's active authority, fulfilling a challenge, restoring posting rights, and making
       * the account immune to challenge for 24 hours.
       *
       * @param challenged The account that was challenged and is proving its authority.
       * @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction prove( string challenged, bool broadcast );

      /**
       *  Account operations have sequence numbers from 0 to N where N is the most recent operation. This method
       *  returns operations in the range [from-limit, from]
       *
       *  @param account - account whose history will be returned
       *  @param from - the absolute sequence number, -1 means most recent, limit is the number of operations before from.
       *  @param limit - the maximum number of items that can be queried (0 to 1000], must be less than from
       */
      map<uint32_t,applied_operation> get_account_history( string account, uint32_t from, uint32_t limit );


      /**
       *  Marks one account as following another account.  Requires the posting authority of the follower.
       *
       *  @param what - a set of things to follow: posts, comments, votes, ignore
       */
      annotated_signed_transaction follow( string follower, string following, set<string> what, bool broadcast );


      std::map<string,std::function<string(fc::variant,const fc::variants&)>> get_result_formatters() const;

      fc::signal<void(bool)> lock_changed;
      std::shared_ptr<detail::wallet_api_impl> my;
      void encrypt_keys();

      /**
       * Checks memos against private keys on account and imported in wallet
       */
      void check_memo( const string& memo, const account_api_obj& account )const;

      /**
       *  Returns the encrypted memo if memo starts with '#' otherwise returns memo
       */
      string get_encrypted_memo( string from, string to, string memo );

      /**
       * Returns the decrypted memo if possible given wallet's known private keys
       */
      string decrypt_memo( string memo );

      annotated_signed_transaction decline_voting_rights( string account, bool decline, bool broadcast );

      annotated_signed_transaction claim_reward_balance( string account, asset reward_futurepia, asset reward_fpch, bool broadcast );

      /**
       *  This method will genrate new token. 
       *
       *  @param creator The account creating new token
       *  @param token_name The name of new token
       *  @param symbol symbol using new token
       *  @param init_supply initial token supply amount
       *  @param broadcast true if you wish to broadcast the transaction
       */
      annotated_signed_transaction create_token( string creator, string token_name, string symbol, int64_t init_supply, bool broadcast );

      /** 
       * Returns the information about token
       * @param name token name
       * @returns token data
       */
      optional< token_api_object > get_token( string name );

      /**
       * Returns token balance list of a account
       * @param account account name
       * @return token balance list
       * */
      vector< token_balance_api_object > get_token_balance( string account );

      /**
       * get token list
       * @param lower_bound_name search keyword
       * @param limit max count to read from db
       * @return token list
       * */
      vector< token_api_object > list_tokens( string lower_bound_name, uint32_t limit );

      /**
       * transfer owned token to another
       * @param from The account the tokens are coming from
       * @param to The account the tokens are going to
       * @param amount The tokens being transferred. i.e. "100.00000000 TEST"
       * @param memo A memo for the transactionm, encrypted with the to account's public memo key
       * @param broadcast true if you wish to broadcast the transaction
       * */
      annotated_signed_transaction transfer_token( string from, string to, asset amount, string memo, bool broadcast = false );	  
      
      /**
       * burn owned token 
       * @param account account that owns token to be burned
       * @param amount token amount to be burned
       * @param broadcast true if you wish to broadcast the transaction
       * */
      annotated_signed_transaction burn_token( string account, asset amount, bool broadcast );

    /**
     * get accounts by token name.
     * @param token_name token name.
     * @return accounts owned the token.
     * */
    vector< token_balance_api_object > get_accounts_by_token( string token_name );
};

struct plain_keys {
   fc::sha512                  checksum;
   map<public_key_type,string> keys;
};

} // namespace futurepia::wallet 
} // namespace futurepia

FC_REFLECT( futurepia::wallet::wallet_data,
            (cipher_keys)
            (ws_server)
            (ws_user)
            (ws_password)
          )

FC_REFLECT( futurepia::wallet::brain_key_info, (brain_priv_key)(wif_priv_key) (pub_key))

FC_REFLECT( futurepia::wallet::plain_keys, (checksum)(keys) )

FC_REFLECT_ENUM( futurepia::wallet::authority_type, (owner)(active)(posting) )

FC_API( futurepia::wallet::wallet_api,
        /// wallet api
        (help)(gethelp)
        (about)(is_new)(is_locked)(lock)(unlock)(set_password)
        (load_wallet_file)(save_wallet_file)

        /// key api
        (import_key)
        (suggest_brain_key)
        (list_keys)
        (get_private_key)
        (get_private_key_from_password)
        (normalize_brain_key)

        /// query api
        (info)
        (list_my_accounts)
        (list_accounts)
        (list_bobservers)
        (get_bobserver)
        (get_account)
        (get_block)
        (get_ops_in_block)
        (get_feed_history)
        (get_conversion_requests)
        (get_account_history)
        (get_state)

        /// transaction api
        (create_account)
        (create_account_with_keys)
        (create_account_delegated)
        (create_account_with_keys_delegated)
        (update_account)
        (update_account_auth_key)
        (update_account_auth_account)
        (update_account_auth_threshold)
        (update_account_meta)
        (update_account_memo_key)
        (transfer)
        (escrow_transfer)
        (escrow_approve)
        (escrow_dispute)
        (escrow_release)
        (convert_fpch)
        (publish_feed)
        (get_order_book)
        (get_open_orders)
        (create_order)
        (cancel_order)
        (set_transaction_expiration)
        (challenge)
        (prove)
        (request_account_recovery)
        (recover_account)
        (change_recovery_account)
        (get_owner_history)
        (transfer_to_savings)
        (transfer_from_savings)
        (cancel_transfer_from_savings)
        (get_encrypted_memo)
        (decrypt_memo)
        (decline_voting_rights)
        (claim_reward_balance)

        // private message api
        (send_private_message)
        (get_inbox)
        (get_outbox)

        /// helper api
        (get_prototype_operation)
        (serialize_transaction)
        (sign_transaction)

        // token plugin and api
        (create_token)
        (get_token)
        (get_token_balance)
        (list_tokens)
        (transfer_token)
        (burn_token)
        (get_accounts_by_token)

        (network_add_nodes)
        (network_get_connected_peers)

        (get_active_bobservers)
        (get_miner_queue)
        (get_transaction)
      )

FC_REFLECT( futurepia::wallet::memo_data, (from)(to)(nonce)(check)(encrypted) )