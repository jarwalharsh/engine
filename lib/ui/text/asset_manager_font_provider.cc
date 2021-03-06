// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/text/asset_manager_font_provider.h"

#include "lib/fxl/logging.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

namespace {

void VectorReleaseProc(const void* ptr, void* context) {
  delete reinterpret_cast<std::vector<uint8_t>*>(context);
}

}  // anonymous namespace

AssetManagerFontProvider::AssetManagerFontProvider(
    fml::RefPtr<blink::AssetManager> asset_manager)
    : asset_manager_(asset_manager) {}

AssetManagerFontProvider::~AssetManagerFontProvider() = default;

// |FontAssetProvider|
size_t AssetManagerFontProvider::GetFamilyCount() const {
  return family_names_.size();
}

// |FontAssetProvider|
std::string AssetManagerFontProvider::GetFamilyName(int index) const {
  FXL_DCHECK(index >= 0 && static_cast<size_t>(index) < family_names_.size());
  return family_names_[index];
}

// |FontAssetProvider|
SkFontStyleSet* AssetManagerFontProvider::MatchFamily(
    const std::string& family_name) {
  auto found = registered_families_.find(family_name);
  if (found == registered_families_.end()) {
    return nullptr;
  }
  return SkRef(&found->second);
}

void AssetManagerFontProvider::RegisterAsset(std::string family_name,
                                             std::string asset) {
  auto family_it = registered_families_.find(family_name);

  if (family_it == registered_families_.end()) {
    family_names_.push_back(family_name);
    family_it = registered_families_
                    .emplace(std::piecewise_construct,
                             std::forward_as_tuple(family_name),
                             std::forward_as_tuple(asset_manager_))
                    .first;
  }

  family_it->second.registerAsset(asset);
}

AssetManagerFontStyleSet::AssetManagerFontStyleSet(
    fml::RefPtr<blink::AssetManager> asset_manager)
    : asset_manager_(asset_manager) {}

AssetManagerFontStyleSet::~AssetManagerFontStyleSet() = default;

void AssetManagerFontStyleSet::registerAsset(std::string asset) {
  assets_.emplace_back(asset);
}

int AssetManagerFontStyleSet::count() {
  return assets_.size();
}

void AssetManagerFontStyleSet::getStyle(int index,
                                        SkFontStyle*,
                                        SkString* style) {
  FXL_DCHECK(false);
}

SkTypeface* AssetManagerFontStyleSet::createTypeface(int i) {
  size_t index = i;
  if (index >= assets_.size())
    return nullptr;

  TypefaceAsset& asset = assets_[index];
  if (!asset.typeface) {
    std::unique_ptr<std::vector<uint8_t>> asset_buf =
        std::make_unique<std::vector<uint8_t>>();
    if (!asset_manager_->GetAsBuffer(asset.asset, asset_buf.get()))
      return nullptr;

    std::vector<uint8_t>* asset_buf_ptr = asset_buf.release();
    sk_sp<SkData> asset_data =
        SkData::MakeWithProc(asset_buf_ptr->data(), asset_buf_ptr->size(),
                             VectorReleaseProc, asset_buf_ptr);
    std::unique_ptr<SkMemoryStream> stream = SkMemoryStream::Make(asset_data);

    // Ownership of the stream is transferred.
    asset.typeface = SkTypeface::MakeFromStream(stream.release());
    if (!asset.typeface)
      return nullptr;
  }

  return SkRef(asset.typeface.get());
}

SkTypeface* AssetManagerFontStyleSet::matchStyle(const SkFontStyle& pattern) {
  if (assets_.empty())
    return nullptr;

  for (const TypefaceAsset& asset : assets_)
    if (asset.typeface && asset.typeface->fontStyle() == pattern)
      return SkRef(asset.typeface.get());

  return SkRef(assets_[0].typeface.get());
}

}  // namespace blink
