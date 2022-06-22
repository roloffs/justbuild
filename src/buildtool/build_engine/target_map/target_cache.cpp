#include "src/buildtool/build_engine/target_map/target_cache.hpp"

#include "src/buildtool/common/repository_config.hpp"
#include "src/buildtool/execution_api/local/config.hpp"
#include "src/buildtool/execution_api/local/file_storage.hpp"
#include "src/buildtool/execution_api/local/local_cas.hpp"
#include "src/buildtool/execution_api/remote/config.hpp"
#include "src/buildtool/file_system/file_system_manager.hpp"
#include "src/buildtool/logging/logger.hpp"

auto TargetCache::Key::Create(BuildMaps::Base::EntityName const& target,
                              Configuration const& effective_config) noexcept
    -> std::optional<TargetCache::Key> {
    static auto const& repos = RepositoryConfig::Instance();
    try {
        if (auto repo_key =
                repos.RepositoryKey(target.GetNamedTarget().repository)) {
            // target's repository is content-fixed, we can compute a cache key
            auto const& name = target.GetNamedTarget();
            auto target_desc = nlohmann::json{
                {{"repo_key", *repo_key},
                 {"target_name", nlohmann::json{name.module, name.name}.dump()},
                 {"effective_config", effective_config.ToString()}}};
            static auto const& cas = LocalCAS<ObjectType::File>::Instance();
            if (auto target_key = cas.StoreBlobFromBytes(target_desc.dump(2))) {
                return Key{
                    {ArtifactDigest{std::move(*target_key)}, ObjectType::File}};
            }
        }
    } catch (std::exception const& ex) {
        Logger::Log(LogLevel::Error,
                    "Creating target cache key failed with:\n{}",
                    ex.what());
    }
    return std::nullopt;
}

auto TargetCache::Entry::FromTarget(
    AnalysedTargetPtr const& target,
    std::unordered_map<ArtifactDescription, Artifact::ObjectInfo> const&
        replacements) noexcept -> std::optional<TargetCache::Entry> {
    auto result = TargetResult{
        target->Artifacts(), target->Provides(), target->RunFiles()};
    if (auto desc = result.ReplaceNonKnownAndToJson(replacements)) {
        return Entry{*desc};
    }
    return std::nullopt;
}

auto TargetCache::Entry::ToResult() const -> std::optional<TargetResult> {
    return TargetResult::FromJson(desc_);
}

auto TargetCache::Store(Key const& key, Entry const& value) const noexcept
    -> bool {
    if (auto digest = CAS().StoreBlobFromBytes(value.ToJson().dump(2))) {
        auto data = Artifact::ObjectInfo{ArtifactDigest{std::move(*digest)},
                                         ObjectType::File}
                        .ToString();
        logger_.Emit(LogLevel::Debug,
                     "Adding entry for key {} as {}",
                     key.Id().ToString(),
                     data);
        return file_store_.AddFromBytes(key.Id().digest.hash(), data);
    }
    return false;
}

auto TargetCache::Read(Key const& key) const noexcept
    -> std::optional<std::pair<Entry, Artifact::ObjectInfo>> {
    auto entry_path = file_store_.GetPath(key.Id().digest.hash());
    auto const entry =
        FileSystemManager::ReadFile(entry_path, ObjectType::File);
    if (not entry.has_value()) {
        logger_.Emit(LogLevel::Debug,
                     "Cache miss, entry not found {}",
                     entry_path.string());
        return std::nullopt;
    }
    if (auto info = Artifact::ObjectInfo::FromString(*entry)) {
        if (auto path = CAS().BlobPath(info->digest)) {
            if (auto value = FileSystemManager::ReadFile(*path)) {
                try {
                    return std::make_pair(Entry{nlohmann::json::parse(*value)},
                                          std::move(*info));
                } catch (std::exception const& ex) {
                    logger_.Emit(LogLevel::Warning,
                                 "Parsing entry for key {} failed with:\n{}",
                                 key.Id().ToString(),
                                 ex.what());
                }
            }
        }
    }
    logger_.Emit(LogLevel::Warning,
                 "Reading entry for key {} failed",
                 key.Id().ToString());
    return std::nullopt;
}

auto TargetCache::ComputeCacheDir() -> std::filesystem::path {
    return LocalExecutionConfig::TargetCacheDir() / ExecutionBackendId();
}

auto TargetCache::ExecutionBackendId() -> std::string {
    auto address = RemoteExecutionConfig::RemoteAddress();
    auto properties = RemoteExecutionConfig::PlatformProperties();
    auto backend_desc = nlohmann::json{
        {"remote_address",
         address ? nlohmann::json{fmt::format(
                       "{}:{}", address->host, address->port)}
                 : nlohmann::json{}},
        {"platform_properties",
         properties}}.dump(2);
    return CAS().StoreBlobFromBytes(backend_desc).value().hash();
}