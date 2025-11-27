/* packageranking.h - Package ranking and trust scoring for PolySynaptic
 *
 * Copyright (c) 2024 PolySynaptic Contributors
 *
 * This file provides a ranking and scoring system for packages from
 * multiple sources. It helps users make informed decisions about which
 * package version to install when the same app is available from
 * multiple providers.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef _PACKAGERANKING_H_
#define _PACKAGERANKING_H_

#include "packagesourceprovider.h"

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace PolySynaptic {

// ============================================================================
// Score Components
// ============================================================================

/**
 * ScoreComponent - Individual factor contributing to package score
 */
struct ScoreComponent {
    std::string name;           // Component name (e.g., "Trust", "Popularity")
    std::string description;    // Human-readable description
    double weight;              // Weight (0.0 - 1.0)
    double rawScore;            // Raw score (0.0 - 1.0)
    double weightedScore;       // weight * rawScore

    ScoreComponent(const std::string& n = "",
                   const std::string& d = "",
                   double w = 0.0,
                   double r = 0.0)
        : name(n)
        , description(d)
        , weight(w)
        , rawScore(r)
        , weightedScore(w * r)
    {}
};

/**
 * PackageScore - Complete score for a package
 */
struct PackageScore {
    std::string packageId;
    std::string providerId;

    // Overall score (0-100)
    int totalScore = 0;

    // Individual components
    std::vector<ScoreComponent> components;

    // Recommendation level
    enum class Recommendation {
        HIGHLY_RECOMMENDED,
        RECOMMENDED,
        ACCEPTABLE,
        CAUTION,
        NOT_RECOMMENDED
    };
    Recommendation recommendation = Recommendation::ACCEPTABLE;

    // Warning messages
    std::vector<std::string> warnings;

    // Advantages of this package version
    std::vector<std::string> advantages;

    // Get recommendation as string
    std::string getRecommendationString() const {
        switch (recommendation) {
            case Recommendation::HIGHLY_RECOMMENDED: return "Highly Recommended";
            case Recommendation::RECOMMENDED: return "Recommended";
            case Recommendation::ACCEPTABLE: return "Acceptable";
            case Recommendation::CAUTION: return "Use with Caution";
            case Recommendation::NOT_RECOMMENDED: return "Not Recommended";
        }
        return "Unknown";
    }

    // Get recommendation color for UI
    std::string getRecommendationColor() const {
        switch (recommendation) {
            case Recommendation::HIGHLY_RECOMMENDED: return "#2e7d32";  // Green
            case Recommendation::RECOMMENDED: return "#558b2f";
            case Recommendation::ACCEPTABLE: return "#f9a825";  // Yellow
            case Recommendation::CAUTION: return "#ef6c00";  // Orange
            case Recommendation::NOT_RECOMMENDED: return "#c62828";  // Red
        }
        return "#757575";  // Grey
    }
};

// ============================================================================
// Ranking Configuration
// ============================================================================

/**
 * RankingConfig - Configurable weights for scoring factors
 */
struct RankingConfig {
    // Trust and security (default: high priority)
    double trustWeight = 0.30;
    double confinementWeight = 0.15;
    double permissionWeight = 0.10;

    // Package quality
    double updateFrequencyWeight = 0.10;
    double versionRecencyWeight = 0.10;

    // User preference
    double providerPreferenceWeight = 0.15;
    double popularityWeight = 0.10;

    // Provider preference order (most preferred first)
    std::vector<std::string> providerPriority = {"apt", "flatpak", "snap"};

    // Trusted publishers/developers
    std::vector<std::string> trustedPublishers;

    // Validate weights sum to 1.0
    bool validate() const {
        double sum = trustWeight + confinementWeight + permissionWeight +
                    updateFrequencyWeight + versionRecencyWeight +
                    providerPreferenceWeight + popularityWeight;
        return (sum >= 0.99 && sum <= 1.01);
    }

    // Normalize weights to sum to 1.0
    void normalize() {
        double sum = trustWeight + confinementWeight + permissionWeight +
                    updateFrequencyWeight + versionRecencyWeight +
                    providerPreferenceWeight + popularityWeight;

        if (sum > 0) {
            trustWeight /= sum;
            confinementWeight /= sum;
            permissionWeight /= sum;
            updateFrequencyWeight /= sum;
            versionRecencyWeight /= sum;
            providerPreferenceWeight /= sum;
            popularityWeight /= sum;
        }
    }
};

// ============================================================================
// Package Ranker
// ============================================================================

/**
 * PackageRanker - Ranks packages and provides recommendations
 */
class PackageRanker {
public:
    PackageRanker();
    explicit PackageRanker(const RankingConfig& config);

    /**
     * Score a single package
     */
    PackageScore scorePackage(const UnifiedPackage& package);

    /**
     * Rank multiple packages (same app from different providers)
     * Returns packages sorted by score (highest first)
     */
    std::vector<PackageScore> rankPackages(
        const std::vector<UnifiedPackage>& packages);

    /**
     * Get the best package from a list
     */
    std::optional<PackageScore> getBestPackage(
        const std::vector<UnifiedPackage>& packages);

    /**
     * Compare two packages
     */
    struct ComparisonResult {
        PackageScore first;
        PackageScore second;
        std::string winner;  // providerId of winner
        std::vector<std::string> reasons;
    };
    ComparisonResult comparePackages(const UnifiedPackage& a,
                                      const UnifiedPackage& b);

    /**
     * Configuration
     */
    void setConfig(const RankingConfig& config) { _config = config; }
    const RankingConfig& getConfig() const { return _config; }

    /**
     * Set custom scoring function for a component
     */
    using ScoringFunction = std::function<double(const UnifiedPackage&)>;
    void setCustomScorer(const std::string& component, ScoringFunction fn);

private:
    RankingConfig _config;
    std::map<std::string, ScoringFunction> _customScorers;

    // Individual scoring functions
    double scoreTrust(const UnifiedPackage& pkg);
    double scoreConfinement(const UnifiedPackage& pkg);
    double scorePermissions(const UnifiedPackage& pkg);
    double scoreUpdateFrequency(const UnifiedPackage& pkg);
    double scoreVersionRecency(const UnifiedPackage& pkg);
    double scoreProviderPreference(const UnifiedPackage& pkg);
    double scorePopularity(const UnifiedPackage& pkg);

    // Determine recommendation level from total score
    PackageScore::Recommendation getRecommendation(int score,
        const UnifiedPackage& pkg);

    // Generate warnings for a package
    std::vector<std::string> generateWarnings(const UnifiedPackage& pkg);

    // Generate advantages for a package
    std::vector<std::string> generateAdvantages(const UnifiedPackage& pkg);
};

// ============================================================================
// Duplicate Detector
// ============================================================================

/**
 * DuplicateGroup - Group of packages that are the same app
 */
struct DuplicateGroup {
    std::string canonicalName;  // Normalized app name
    std::vector<UnifiedPackage> packages;
    std::optional<PackageScore> recommended;
};

/**
 * DuplicateDetector - Finds duplicate packages across providers
 */
class DuplicateDetector {
public:
    /**
     * Find duplicate packages in a list
     */
    std::vector<DuplicateGroup> findDuplicates(
        const std::vector<UnifiedPackage>& packages);

    /**
     * Check if two packages are the same app
     */
    bool isSameApp(const UnifiedPackage& a, const UnifiedPackage& b);

    /**
     * Get canonical name for comparison
     */
    std::string getCanonicalName(const UnifiedPackage& pkg);

private:
    // Normalize name for comparison
    std::string normalizeName(const std::string& name);

    // Known app ID mappings (snap name -> flatpak ID -> apt package)
    static const std::map<std::string, std::vector<std::string>> knownMappings;
};

// ============================================================================
// Installation Advisor
// ============================================================================

/**
 * InstallationAdvice - Recommendation for installing an app
 */
struct InstallationAdvice {
    std::string appName;

    // Primary recommendation
    PackageScore primary;

    // Alternative options
    std::vector<PackageScore> alternatives;

    // Overall advice text
    std::string adviceText;

    // Should the user be warned before installing?
    bool requiresConfirmation = false;
    std::string confirmationReason;
};

/**
 * InstallationAdvisor - Provides installation recommendations
 */
class InstallationAdvisor {
public:
    InstallationAdvisor();
    explicit InstallationAdvisor(std::shared_ptr<PackageRanker> ranker);

    /**
     * Get advice for installing an app
     */
    InstallationAdvice getAdvice(const std::vector<UnifiedPackage>& packages);

    /**
     * Check if an app should be installed from a different source
     */
    struct MigrationAdvice {
        bool shouldMigrate = false;
        std::string currentProviderId;
        std::string recommendedProviderId;
        std::string reason;
    };
    MigrationAdvice checkMigration(const UnifiedPackage& installed,
                                    const std::vector<UnifiedPackage>& available);

    /**
     * Set the ranker to use
     */
    void setRanker(std::shared_ptr<PackageRanker> ranker) {
        _ranker = std::move(ranker);
    }

private:
    std::shared_ptr<PackageRanker> _ranker;

    std::string generateAdviceText(const PackageScore& primary,
                                    const std::vector<PackageScore>& alternatives);
};

} // namespace PolySynaptic

#endif // _PACKAGERANKING_H_

// vim:ts=4:sw=4:et
