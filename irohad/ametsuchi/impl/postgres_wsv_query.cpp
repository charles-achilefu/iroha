/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ametsuchi/impl/postgres_wsv_query.hpp"

namespace iroha {
  namespace ametsuchi {

    using std::string;

    using nonstd::optional;
    using nonstd::nullopt;
    using model::Account;
    using model::Asset;
    using model::AccountAsset;
    using model::Peer;

    PostgresWsvQuery::PostgresWsvQuery(pqxx::nontransaction &transaction)
        : transaction_(transaction), log_(logger::log("PostgresWsvQuery")) {}

    bool PostgresWsvQuery::hasAccountGrantablePermission(
        const std::string &permitee_account_id, const std::string &account_id,
        const std::string &permission_id) {
      pqxx::result result;
      try {
        result = transaction_.exec(
            "SELECT * FROM account_has_grantable_permissions WHERE "
            "permittee_account_id = " +
            transaction_.quote(permitee_account_id) + " AND account_id = " +
            transaction_.quote(account_id) + " AND permission_id = " +
            transaction_.quote(permission_id) + ";");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return false;
      }
      return result.size() == 1;
    };

    nonstd::optional<std::vector<std::string>>
    PostgresWsvQuery::getAccountRoles(const std::string &account_id) {
      pqxx::result result;
      try {
        result = transaction_.exec(
            "SELECT role_id FROM account_has_roles WHERE account_id = " +
            transaction_.quote(account_id) + ";");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      std::vector<std::string> roles;
      for (const auto &row : result) {
        roles.emplace_back(row.at("role_id").c_str());
      }
      return roles;
    };

    nonstd::optional<std::vector<std::string>>
    PostgresWsvQuery::getRolePermissions(const std::string &role_name) {
      pqxx::result result;
      try {
        result = transaction_.exec(
            "SELECT permission_id FROM role_has_permissions WHERE role_id = " +
            transaction_.quote(role_name) + ";");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      std::vector<std::string> permissions;
      for (const auto &row : result) {
        permissions.emplace_back(row.at("permission_id").c_str());
      }
      return permissions;
    };

    nonstd::optional<std::vector<std::string>> PostgresWsvQuery::getRoles() {
      pqxx::result result;
      try {
        result = transaction_.exec("SELECT role_id FROM role;");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      std::vector<std::string> roles;
      for (const auto &row : result) {
        roles.emplace_back(row.at("role_id").c_str());
      }
      return roles;
    };

    optional<Account> PostgresWsvQuery::getAccount(const string &account_id) {
      pqxx::result result;
      try {
        result = transaction_.exec("SELECT * FROM account WHERE account_id = "
                                   + transaction_.quote(account_id) + ";");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      if (result.empty()) {
        log_->info("Account {} not found", account_id);
        return nullopt;
      }
      Account account;
      auto row = result.at(0);
      row.at("account_id") >> account.account_id;
      row.at("domain_id") >> account.domain_name;
      row.at("quorum") >> account.quorum;
      //      row.at("transaction_count") >> ?
      std::string permissions;
      row.at("permissions") >> permissions;
      account.permissions.add_signatory = permissions.at(0) - '0';
      account.permissions.can_transfer = permissions.at(1) - '0';
      account.permissions.create_accounts = permissions.at(2) - '0';
      account.permissions.create_assets = permissions.at(3) - '0';
      account.permissions.create_domains = permissions.at(4) - '0';
      account.permissions.issue_assets = permissions.at(5) - '0';
      account.permissions.read_all_accounts = permissions.at(6) - '0';
      account.permissions.remove_signatory = permissions.at(7) - '0';
      account.permissions.set_permissions = permissions.at(8) - '0';
      account.permissions.set_quorum = permissions.at(9) - '0';
      return account;
    }

    nonstd::optional<std::vector<pubkey_t>>
    PostgresWsvQuery::getSignatories(const string &account_id) {
      pqxx::result result;
      try {
        result = transaction_.exec(
            "SELECT public_key FROM account_has_signatory WHERE account_id = "
            + transaction_.quote(account_id) + ";");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      std::vector<pubkey_t> signatories;
      for (const auto &row : result) {
        pqxx::binarystring public_key_str(row.at("public_key"));
        pubkey_t pubkey;
        std::copy(public_key_str.begin(), public_key_str.end(), pubkey.begin());
        signatories.push_back(pubkey);
      }
      return signatories;
    }

    optional<Asset> PostgresWsvQuery::getAsset(const string &asset_id) {
      pqxx::result result;
      try {
        result = transaction_.exec("SELECT * FROM asset WHERE asset_id = "
                                   + transaction_.quote(asset_id) + ";");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      if (result.empty()) {
        log_->info("Asset {} not found", asset_id);
        return nullopt;
      }
      Asset asset;
      auto row = result.at(0);
      row.at("asset_id") >> asset.asset_id;
      row.at("domain_id") >> asset.domain_id;
      int32_t precision;
      row.at("precision") >> precision;
      asset.precision = precision;
      //      row.at("data") >> ?
      return asset;
    }

    optional<AccountAsset> PostgresWsvQuery::getAccountAsset(
        const std::string &account_id, const std::string &asset_id) {
      pqxx::result result;
      try {
        result = transaction_.exec(
            "SELECT * FROM account_has_asset WHERE account_id = "
            + transaction_.quote(account_id) + " AND asset_id = "
            + transaction_.quote(asset_id) + ";");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      if (result.empty()) {
        log_->info("Account {} does not have asset {}", account_id, asset_id);
        return nullopt;
      }
      model::AccountAsset asset;
      auto row = result.at(0);
      row.at("account_id") >> asset.account_id;
      row.at("asset_id") >> asset.asset_id;
      std::string amount_str;
      row.at("amount") >> amount_str;
      asset.balance = Amount::createFromString(amount_str).value();
      return asset;
    }

    nonstd::optional<std::vector<model::Peer>> PostgresWsvQuery::getPeers() {
      pqxx::result result;
      try {
        result = transaction_.exec("SELECT * FROM peer;");
      } catch (const std::exception &e) {
        log_->error(e.what());
        return nullopt;
      }
      std::vector<Peer> peers;
      for (const auto &row : result) {
        model::Peer peer;
        pqxx::binarystring public_key_str(row.at("public_key"));
        pubkey_t pubkey;
        std::copy(public_key_str.begin(), public_key_str.end(), pubkey.begin());
        peer.pubkey = pubkey;
        row.at("address") >> peer.address;
        peers.push_back(peer);
      }
      return peers;
    }
  }  // namespace ametsuchi
}  // namespace iroha