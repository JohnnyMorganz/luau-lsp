--- Script to publish the Luau Language Server companion plugin to Roblox

local AssetService = game:GetService("AssetService")

local ASSET_ID = 10913122509
local CREATOR_ID = 68136726

print("Executing AssetService:CreateAssetVersionAsync")

local result, number =
	AssetService:CreateAssetVersionAsync(game.ReplicatedStorage.Plugin, Enum.AssetType.Plugin, ASSET_ID, {
		CreatorId = CREATOR_ID,
	})

print(result, number)
