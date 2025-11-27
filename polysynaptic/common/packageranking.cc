/* packageranking.cc - Package ranking and trust scoring implementation
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "packageranking.h"

#include <algorithm>
#include <cmath>
#include <regex>

namespace PolySynaptic {

// ============================================================================
// PackageRanker Implementation
// ============================================================================

PackageRanker::PackageRanker() : _config() {
    _config.normalize();
}

PackageRanker::PackageRanker(const RankingConfig& config) : _config(config) {
    _config.normalize();
}

PackageScore PackageRanker::scorePackage(const UnifiedPackage& package) {
    PackageScore score;
    score.packageId = package.id;
    score.providerId = package.providerId;

    // Calculate individual components
    score.components.push_back(ScoreComponent(
        "Trust",
        "Publisher trust and verification status",
        _config.trustWeight,
        scoreTrust(package)));

    score.components.push_back(ScoreComponent(
        "Confinement",
        "Sandboxing and isolation level",
        _config.confinementWeight,
        scoreConfinement(package)));

    score.components.push_back(ScoreComponent(
        "Permissions",
        "Requested permissions and access",
        _config.permissionWeight,
        scorePermissions(package)));

    score.components.push_back(ScoreComponent(
        "Update Frequency",
        "How frequently the package is updated",
        _config.updateFrequencyWeight,
        scoreUpdateFrequency(package)));

    score.components.push_back(ScoreComponent(
        "Version Recency",
        "How recent the available version is",
        _config.versionRecencyWeight,
        scoreVersionRecency(package)));

    score.components.push_back(ScoreComponent(
        "Provider Preference",
        "User's preferred package source",
        _config.providerPreferenceWeight,
        scoreProviderPreference(package)));

    score.components.push_back(ScoreComponent(
        "Popularity",
        "Usage and community adoption",
        _config.popularityWeight,
        scorePopularity(package)));

    // Calculate total score
    double totalWeighted = 0.0;
    for (const auto& comp : score.components) {
        totalWeighted += comp.weightedScore;
    }

    score.totalScore = static_cast<int>(std::round(totalWeighted * 100));
    score.totalScore = std::max(0, std::min(100, score.totalScore));

    // Determine recommendation
    score.recommendation = getRecommendation(score.totalScore, package);

    // Generate warnings and advantages
    score.warnings = generateWarnings(package);
    score.advantages = generateAdvantages(package);

    return score;
}

std::vector<PackageScore> PackageRanker::rankPackages(
    const std::vector<UnifiedPackage>& packages)
{
    std::vector<PackageScore> scores;
    scores.reserve(packages.size());

    for (const auto& pkg : packages) {
        scores.push_back(scorePackage(pkg));
    }

    // Sort by total score (descending)
    std::sort(scores.begin(), scores.end(),
              [](const PackageScore& a, const PackageScore& b) {
                  return a.totalScore > b.totalScore;
              });

    return scores;
}

std::optional<PackageScore> PackageRanker::getBestPackage(
    const std::vector<UnifiedPackage>& packages)
{
    if (packages.empty()) {
        return std::nullopt;
    }

    auto ranked = rankPackages(packages);
    return ranked.front();
}

PackageRanker::ComparisonResult PackageRanker::comparePackages(
    const UnifiedPackage& a,
    const UnifiedPackage& b)
{
    ComparisonResult result;
    result.first = scorePackage(a);
    result.second = scorePackage(b);

    if (result.first.totalScore > result.second.totalScore) {
        result.winner = a.providerId;
    } else if (result.second.totalScore > result.first.totalScore) {
        result.winner = b.providerId;
    } else {
        result.winner = "";  // Tie
    }

    // Generate comparison reasons
    for (size_t i = 0; i < result.first.components.size() &&
                       i < result.second.components.size(); i++) {
        const auto& compA = result.first.components[i];
        const auto& compB = result.second.components[i];

        double diff = compA.rawScore - compB.rawScore;
        if (std::abs(diff) > 0.1) {
            std::string reason = compA.name + ": ";
            if (diff > 0) {
                reason += a.providerId + " scores higher";
            } else {
                reason += b.providerId + " scores higher";
            }
            result.reasons.push_back(reason);
        }
    }

    return result;
}

void PackageRanker::setCustomScorer(const std::string& component,
                                     ScoringFunction fn)
{
    _customScorers[component] = std::move(fn);
}

// ============================================================================
// Individual Scoring Functions
// ============================================================================

double PackageRanker::scoreTrust(const UnifiedPackage& pkg) {
    if (_customScorers.count("Trust") > 0) {
        return _customScorers["Trust"](pkg);
    }

    switch (pkg.trustLevel) {
        case TrustLevel::OFFICIAL:
            return 1.0;
        case TrustLevel::VERIFIED:
            return 0.85;
        case TrustLevel::COMMUNITY:
            return 0.6;
        case TrustLevel::THIRD_PARTY:
            return 0.4;
        case TrustLevel::UNKNOWN:
        default:
            return 0.2;
    }
}

double PackageRanker::scoreConfinement(const UnifiedPackage& pkg) {
    if (_customScorers.count("Confinement") > 0) {
        return _customScorers["Confinement"](pkg);
    }

    switch (pkg.confinement) {
        case ConfinementLevel::STRICT:
            return 1.0;
        case ConfinementLevel::CLASSIC:
            return 0.5;
        case ConfinementLevel::DEVMODE:
            return 0.3;
        case ConfinementLevel::UNCONFINED:
            // APT packages are unconfined but this isn't necessarily bad
            // as they're usually from trusted repos
            if (pkg.providerId == "apt") {
                return 0.7;  // APT gets partial credit due to repo trust
            }
            return 0.2;
        case ConfinementLevel::UNKNOWN:
        default:
            return 0.4;
    }
}

double PackageRanker::scorePermissions(const UnifiedPackage& pkg) {
    if (_customScorers.count("Permissions") > 0) {
        return _customScorers["Permissions"](pkg);
    }

    // APT packages have full access, but from trusted sources
    if (pkg.providerId == "apt") {
        return 0.7;  // Middle ground
    }

    // For Snap/Flatpak, fewer permissions = higher score
    size_t permCount = pkg.permissions.permissions.size();

    if (permCount == 0) {
        return 1.0;  // No special permissions
    } else if (permCount <= 3) {
        return 0.9;
    } else if (permCount <= 5) {
        return 0.7;
    } else if (permCount <= 10) {
        return 0.5;
    } else {
        return 0.3;  // Many permissions
    }
}

double PackageRanker::scoreUpdateFrequency(const UnifiedPackage& pkg) {
    if (_customScorers.count("UpdateFrequency") > 0) {
        return _customScorers["UpdateFrequency"](pkg);
    }

    // This would ideally check actual update history
    // For now, give providers default scores based on typical update patterns

    if (pkg.providerId == "flatpak") {
        return 0.9;  // Flatpak apps are often updated directly by devs
    } else if (pkg.providerId == "snap") {
        return 0.85;  // Snaps auto-update
    } else if (pkg.providerId == "apt") {
        return 0.7;  // APT depends on distro maintainers
    }

    return 0.5;
}

double PackageRanker::scoreVersionRecency(const UnifiedPackage& pkg) {
    if (_customScorers.count("VersionRecency") > 0) {
        return _customScorers["VersionRecency"](pkg);
    }

    // Would ideally compare versions across providers
    // For now, give default scores based on typical patterns

    if (pkg.providerId == "flatpak" || pkg.providerId == "snap") {
        return 0.9;  // Usually have latest versions
    } else if (pkg.providerId == "apt") {
        return 0.6;  // Often behind upstream
    }

    return 0.5;
}

double PackageRanker::scoreProviderPreference(const UnifiedPackage& pkg) {
    if (_customScorers.count("ProviderPreference") > 0) {
        return _customScorers["ProviderPreference"](pkg);
    }

    // Find position in preference list
    const auto& prefs = _config.providerPriority;
    auto it = std::find(prefs.begin(), prefs.end(), pkg.providerId);

    if (it == prefs.end()) {
        return 0.3;  // Not in preference list
    }

    size_t pos = std::distance(prefs.begin(), it);
    size_t total = prefs.size();

    // Score decreases with position (first = 1.0, last = 0.4)
    return 1.0 - (pos * 0.6 / total);
}

double PackageRanker::scorePopularity(const UnifiedPackage& pkg) {
    if (_customScorers.count("Popularity") > 0) {
        return _customScorers["Popularity"](pkg);
    }

    // Would ideally use download counts, ratings, etc.
    // For now, return neutral score
    return 0.5;
}

PackageScore::Recommendation PackageRanker::getRecommendation(
    int score,
    const UnifiedPackage& pkg)
{
    // Check for disqualifying factors
    if (pkg.confinement == ConfinementLevel::DEVMODE) {
        return PackageScore::Recommendation::CAUTION;
    }

    if (pkg.trustLevel == TrustLevel::UNKNOWN) {
        return PackageScore::Recommendation::CAUTION;
    }

    // Score-based recommendation
    if (score >= 85) {
        return PackageScore::Recommendation::HIGHLY_RECOMMENDED;
    } else if (score >= 70) {
        return PackageScore::Recommendation::RECOMMENDED;
    } else if (score >= 50) {
        return PackageScore::Recommendation::ACCEPTABLE;
    } else if (score >= 30) {
        return PackageScore::Recommendation::CAUTION;
    } else {
        return PackageScore::Recommendation::NOT_RECOMMENDED;
    }
}

std::vector<std::string> PackageRanker::generateWarnings(
    const UnifiedPackage& pkg)
{
    std::vector<std::string> warnings;

    // Trust warnings
    if (pkg.trustLevel == TrustLevel::UNKNOWN) {
        warnings.push_back("Publisher is not verified");
    } else if (pkg.trustLevel == TrustLevel::THIRD_PARTY) {
        warnings.push_back("Package is from a third-party source");
    }

    // Confinement warnings
    if (pkg.confinement == ConfinementLevel::CLASSIC) {
        warnings.push_back("Runs without sandboxing (classic confinement)");
    } else if (pkg.confinement == ConfinementLevel::DEVMODE) {
        warnings.push_back("Development mode - not suitable for production");
    }

    // Permission warnings
    bool hasNetworkAccess = false;
    bool hasFullFilesystem = false;

    for (const auto& perm : pkg.permissions.permissions) {
        if (perm.name.find("network") != std::string::npos) {
            hasNetworkAccess = true;
        }
        if (perm.name.find("home") != std::string::npos ||
            perm.name.find("host") != std::string::npos) {
            hasFullFilesystem = true;
        }
    }

    if (hasNetworkAccess && hasFullFilesystem) {
        warnings.push_back("Has network access and can read your files");
    }

    return warnings;
}

std::vector<std::string> PackageRanker::generateAdvantages(
    const UnifiedPackage& pkg)
{
    std::vector<std::string> advantages;

    // Trust advantages
    if (pkg.trustLevel == TrustLevel::OFFICIAL) {
        advantages.push_back("From official distribution repositories");
    } else if (pkg.trustLevel == TrustLevel::VERIFIED) {
        advantages.push_back("Verified publisher");
    }

    // Confinement advantages
    if (pkg.confinement == ConfinementLevel::STRICT) {
        advantages.push_back("Runs in a secure sandbox");
    }

    // Provider-specific advantages
    if (pkg.providerId == "apt") {
        advantages.push_back("Well-tested with your system");
        advantages.push_back("Integrated with system package manager");
    } else if (pkg.providerId == "flatpak") {
        advantages.push_back("Isolated from system");
        advantages.push_back("Usually latest version");
    } else if (pkg.providerId == "snap") {
        advantages.push_back("Automatic updates");
        advantages.push_back("Works across distributions");
    }

    return advantages;
}

// ============================================================================
// DuplicateDetector Implementation
// ============================================================================

const std::map<std::string, std::vector<std::string>>
DuplicateDetector::knownMappings = {
    // Firefox
    {"firefox", {"org.mozilla.firefox", "firefox"}},
    // Chromium
    {"chromium", {"org.chromium.Chromium", "chromium-browser"}},
    // LibreOffice
    {"libreoffice", {"org.libreoffice.LibreOffice", "libreoffice"}},
    // VLC
    {"vlc", {"org.videolan.VLC", "vlc"}},
    // GIMP
    {"gimp", {"org.gimp.GIMP", "gimp"}},
    // VS Code
    {"code", {"com.visualstudio.code", "code"}},
    // Spotify
    {"spotify", {"com.spotify.Client", "spotify-client"}},
    // Slack
    {"slack", {"com.slack.Slack", "slack-desktop"}},
    // Discord
    {"discord", {"com.discordapp.Discord", "discord"}},
    // Telegram
    {"telegram-desktop", {"org.telegram.desktop", "telegram-desktop"}},
};

std::vector<DuplicateGroup> DuplicateDetector::findDuplicates(
    const std::vector<UnifiedPackage>& packages)
{
    std::map<std::string, DuplicateGroup> groups;

    for (const auto& pkg : packages) {
        std::string canonical = getCanonicalName(pkg);

        if (groups.count(canonical) == 0) {
            groups[canonical].canonicalName = canonical;
        }

        groups[canonical].packages.push_back(pkg);
    }

    // Filter to only groups with multiple packages
    std::vector<DuplicateGroup> result;
    for (auto& [name, group] : groups) {
        if (group.packages.size() > 1) {
            // Rank and find recommended
            PackageRanker ranker;
            auto best = ranker.getBestPackage(group.packages);
            group.recommended = best;

            result.push_back(std::move(group));
        }
    }

    return result;
}

bool DuplicateDetector::isSameApp(const UnifiedPackage& a,
                                   const UnifiedPackage& b)
{
    if (a.providerId == b.providerId) {
        return a.id == b.id;
    }

    return getCanonicalName(a) == getCanonicalName(b);
}

std::string DuplicateDetector::getCanonicalName(const UnifiedPackage& pkg) {
    std::string normalized = normalizeName(pkg.id);

    // Check known mappings
    for (const auto& [canonical, variants] : knownMappings) {
        for (const auto& variant : variants) {
            if (normalizeName(variant) == normalized) {
                return canonical;
            }
        }
        if (canonical == normalized) {
            return canonical;
        }
    }

    // For Flatpak IDs, extract app name
    if (pkg.providerId == "flatpak") {
        // org.example.AppName -> appname
        size_t lastDot = pkg.id.rfind('.');
        if (lastDot != std::string::npos) {
            return normalizeName(pkg.id.substr(lastDot + 1));
        }
    }

    return normalized;
}

std::string DuplicateDetector::normalizeName(const std::string& name) {
    std::string normalized = name;

    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(),
                  normalized.begin(), ::tolower);

    // Remove common suffixes
    static std::vector<std::string> suffixes = {
        "-desktop", "-browser", "-client", "-app"
    };

    for (const auto& suffix : suffixes) {
        if (normalized.length() > suffix.length() &&
            normalized.substr(normalized.length() - suffix.length()) == suffix) {
            normalized = normalized.substr(0, normalized.length() - suffix.length());
        }
    }

    // Remove hyphens and underscores for comparison
    normalized.erase(
        std::remove_if(normalized.begin(), normalized.end(),
                      [](char c) { return c == '-' || c == '_' || c == '.'; }),
        normalized.end());

    return normalized;
}

// ============================================================================
// InstallationAdvisor Implementation
// ============================================================================

InstallationAdvisor::InstallationAdvisor()
    : _ranker(std::make_shared<PackageRanker>())
{}

InstallationAdvisor::InstallationAdvisor(std::shared_ptr<PackageRanker> ranker)
    : _ranker(std::move(ranker))
{}

InstallationAdvice InstallationAdvisor::getAdvice(
    const std::vector<UnifiedPackage>& packages)
{
    InstallationAdvice advice;

    if (packages.empty()) {
        advice.adviceText = "No packages available";
        return advice;
    }

    // Get common name from first package
    if (!packages.empty()) {
        advice.appName = packages[0].name;
    }

    // Rank all packages
    auto ranked = _ranker->rankPackages(packages);

    if (ranked.empty()) {
        advice.adviceText = "Unable to rank packages";
        return advice;
    }

    // Primary recommendation
    advice.primary = ranked[0];

    // Alternatives
    for (size_t i = 1; i < ranked.size(); i++) {
        advice.alternatives.push_back(ranked[i]);
    }

    // Generate advice text
    advice.adviceText = generateAdviceText(advice.primary, advice.alternatives);

    // Check if confirmation is needed
    if (advice.primary.recommendation == PackageScore::Recommendation::CAUTION ||
        advice.primary.recommendation == PackageScore::Recommendation::NOT_RECOMMENDED) {
        advice.requiresConfirmation = true;
        advice.confirmationReason = "This package has some concerns. "
                                    "Are you sure you want to install it?";
    }

    if (!advice.primary.warnings.empty()) {
        advice.requiresConfirmation = true;
        advice.confirmationReason = advice.primary.warnings[0];
    }

    return advice;
}

InstallationAdvisor::MigrationAdvice InstallationAdvisor::checkMigration(
    const UnifiedPackage& installed,
    const std::vector<UnifiedPackage>& available)
{
    MigrationAdvice advice;
    advice.currentProviderId = installed.providerId;

    auto best = _ranker->getBestPackage(available);
    if (!best) {
        return advice;
    }

    advice.recommendedProviderId = best->providerId;

    // Check if migration would be beneficial
    auto currentScore = _ranker->scorePackage(installed);

    int scoreDiff = best->totalScore - currentScore.totalScore;

    if (scoreDiff >= 15 && best->providerId != installed.providerId) {
        advice.shouldMigrate = true;
        advice.reason = "A better version is available from " +
                       best->providerId + " (score: " +
                       std::to_string(best->totalScore) + " vs " +
                       std::to_string(currentScore.totalScore) + ")";
    }

    return advice;
}

std::string InstallationAdvisor::generateAdviceText(
    const PackageScore& primary,
    const std::vector<PackageScore>& alternatives)
{
    std::string text;

    text = "Recommended: Install from " + primary.providerId;
    text += " (Score: " + std::to_string(primary.totalScore) + "/100)";

    if (!primary.advantages.empty()) {
        text += "\n\nAdvantages:";
        for (const auto& adv : primary.advantages) {
            text += "\n  - " + adv;
        }
    }

    if (!primary.warnings.empty()) {
        text += "\n\nNote:";
        for (const auto& warn : primary.warnings) {
            text += "\n  - " + warn;
        }
    }

    if (!alternatives.empty()) {
        text += "\n\nAlternatives:";
        for (const auto& alt : alternatives) {
            text += "\n  - " + alt.providerId +
                   " (Score: " + std::to_string(alt.totalScore) + ")";
        }
    }

    return text;
}

} // namespace PolySynaptic

// vim:ts=4:sw=4:et
