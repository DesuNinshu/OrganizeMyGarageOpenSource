﻿#include "pch.h"
#include "InventoryModel.h"

#include <ranges>


InventoryModel::InventoryModel(std::shared_ptr<GameWrapper> gw): m_gw(std::move(gw))
{
	auto items = m_gw->GetItemsWrapper();
	specialEditionDb = items.GetSpecialEditionDB();
	paintDatabase = items.GetPaintDB();
	LOG("Items::Init-slot-icons");
	InitSlotIcons();
	LOG("Items::Init-products");
	InitProducts();
}

void InventoryModel::InitSlotIcons()
{
	auto iconFolder = m_gw->GetDataFolder() / "OrganizeMyGarageOS";
	m_slotIcons[0] = std::make_shared<ImageWrapper>(iconFolder / "Body.png", false, true);
	m_slotIcons[1] = std::make_shared<ImageWrapper>(iconFolder / "Skin.png", false, true);
	m_slotIcons[2] = std::make_shared<ImageWrapper>(iconFolder / "Wheels.png", false, true);
	m_slotIcons[3] = std::make_shared<ImageWrapper>(iconFolder / "Boost.png", false, true);
	m_slotIcons[4] = std::make_shared<ImageWrapper>(iconFolder / "Antenna.png", false, true);
	m_slotIcons[5] = std::make_shared<ImageWrapper>(iconFolder / "Hat.png", false, true);
	m_slotIcons[7] = std::make_shared<ImageWrapper>(iconFolder / "Paint.png", false, true);
	m_slotIcons[12] = m_slotIcons[7];
	m_slotIcons[13] = std::make_shared<ImageWrapper>(iconFolder / "EngineAudio.png", false, true);
	m_slotIcons[14] = std::make_shared<ImageWrapper>(iconFolder / "Trail.png", false, true);
	m_slotIcons[15] = std::make_shared<ImageWrapper>(iconFolder / "GoalFX.png", false, true);
}

OnlineProdData InventoryModel::GetProdData(OnlineProductWrapper& onlineProd)
{
	OnlineProdData data;
	if (!onlineProd)
	{
		DEBUGLOG("null online product");
		return data;
	}

	auto productWrapper = onlineProd.GetProduct();
	data.SetOfflineProductData(productWrapper);
	data.instanceId = onlineProd.GetInstanceIDV2();

	std::string seEdition;

	auto attributes = onlineProd.GetAttributes();
	for(auto attribute : attributes)
	{
		auto attributeType = attribute.GetAttributeType();
		if (attributeType == "ProductAttribute_Painted_TA")
		{
			auto paintAttribute = ProductAttribute_PaintedWrapper(attribute.memory_address);
			const auto paintId = paintAttribute.GetPaintID();
			auto label = paintAttribute.GetSortLabel().ToString();
			if (label.find("Painted") != std::string::npos)
			{
				data.paint = label.substr(7);
			}
			//data.paint = paintDatabase.GetPaintName(paintId);
			continue;
		}
		if (attributeType == "ProductAttribute_SpecialEdition_TA")
		{
			if (auto specialEditionAttribute = ProductAttribute_SpecialEditionWrapper(attribute.memory_address))
			{
				auto label = specialEditionDb.GetSpecialEditionName(specialEditionAttribute.GetEditionID());
				if (label.find("Edition_") != std::string::npos)
				{
					seEdition = label.substr(8) + " ";
				}
			}
		}
	}
	if (!data.paint.empty() || !seEdition.empty() )
	{
		data.name += " (" + seEdition + data.paint + ")";
	}

	return data;
}

OnlineProdData InventoryModel::GetProdData(const ProductInstanceID& instanceId)
{
	auto items = m_gw->GetItemsWrapper();
	if (!items) return {};
	if (auto onlineProduct = items.GetOnlineProduct(instanceId.lower_bits))
	{
		return GetProdData(onlineProduct);
	}
	OnlineProdData item;
	const auto productId = instanceId.lower_bits;
	if (auto product = items.GetProduct(productId))
	{
		// Non default items can't be equipped without a valid instance id..
		if (product.GetUnlockMethod() != 0 /*EUnlockMethod_UnlockMethod_Default*/)
		{
			return item;
		}
		item.SetOfflineProductData(product);
		item.instanceId = instanceId;
	} else
	{
		DEBUGLOG("no product: {}:{}", static_cast<long>(instanceId.lower_bits), static_cast<long>(instanceId.upper_bits));
	}
	return item;
}


void InventoryModel::InitProducts()
{
	auto items = m_gw->GetItemsWrapper();

	auto onlineProductWrappers = items.GetOwnedProducts();
	auto c = onlineProductWrappers.Count();
	LOG("OwnedProducts: {}", c);
	
	for (auto onlineProd : onlineProductWrappers)
	{
		auto data = GetProdData(onlineProd);
		m_products[data.slot].push_back(data);
	}
	auto productWrappers = items.GetCachedUnlockedProducts();
	auto c2 = productWrappers.Count();
	LOG("CachedUnlockedProduct: {}", c);
	for (auto unlockedProduct : productWrappers)
	{
		if (unlockedProduct.IsNull()) continue;
		unsigned long long id = unlockedProduct.GetID();
		auto data = GetProdData(ProductInstanceID{0, id});
		if (data.prodId != 0)
		{
			m_products[data.slot].push_back(data);
		}
	}

	for (auto& [slotIndex, slotProducts] : m_products)
	{
		// TODO add DefaultProduct_New to slot wrapper?
		//auto slot = GetSlot(slotIndex);
		//if (slot && !slot->_DefaultProduct_New())

		static std::set noDefaultItemSlots = {1, 4, 5, 13};
		if (noDefaultItemSlots.contains(slotIndex))
		{
			OnlineProdData defaultItem;
			defaultItem.name = " None";
			defaultItem.slot = slotIndex;
			slotProducts.push_back(defaultItem);
		}
		std::ranges::sort(slotProducts, {}, &OnlineProdData::name);
	}

	m_products[12] = m_products[7];
	for (auto& data : m_products[12])
	{
		data.slot = 12;
	}
}

const std::set<int>& InventoryModel::GetForcedSlotForBody(int bodyId)
{
	//TODO add body asset equip profile wrappers to make this feature work again
	if (auto it = m_forcedProductSlotCache.find(bodyId); it != m_forcedProductSlotCache.end())
	{
		return it->second;
	}
	static std::set<int> emptySet {};
	return emptySet;
}

std::shared_ptr<ImageWrapper> InventoryModel::GetSlotIcon(int slotIndex)
{
	if (const auto it = m_slotIcons.find(slotIndex); it != m_slotIcons.end())
	{
		return it->second;
	}
	return nullptr;

}

const std::vector<OnlineProdData>& InventoryModel::GetSlotProducts(int slot)
{
	if (auto it = m_products.find(slot); it != m_products.end())
	{
		return it->second;
	}
	static std::vector<OnlineProdData> emptyVector {};
	return emptyVector;
}
