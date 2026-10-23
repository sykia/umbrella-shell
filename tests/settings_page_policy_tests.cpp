#include "core/settings/SettingsPagePolicy.h"
#include <iostream>
using namespace umbrella::settings;
int main() {
    int failures = 0;
    const Rect client{100, 100, 1100, 900};
    std::vector<Node> tree{
        {-1, L"root", client, true},
        {0, L"nav", {100, 100, 350, 900}, true, false, false, true},
        {0, L"search", {360, 100, 1000, 140}, true, false, false, true},
        {0, L"header", {380, 160, 1000, 210}, true, true},
        {0, L"viewport", {380, 220, 1080, 880}, true, false, true},
        {4, L"SystemSettings_DesktopTaskbar_Search_EntityItem", {400, 230, 1060, 330}, true},
        {4, L"SystemSettings_DesktopTaskbar_TaskView_EntityItem", {400, 350, 1060, 450}, true}};
    const auto accepted = contentBounds(tree, client, true);
    if (!accepted || accepted->top != 220 || accepted->left != 380) ++failures;
    if (contentBounds(tree, client, false)) ++failures;
    auto bad = tree; bad[6].id = L"SystemSettings_Personalize_TaskbarSettingLink";
    if (contentBounds(bad, client, true)) ++failures;
    bad = tree; bad[6].id = bad[5].id;
    if (contentBounds(bad, client, true)) ++failures;
    bad = tree; bad[6].id = L"SystemSettings_DesktopTaskbar_Search_ComboBox";
    if (contentBounds(bad, client, true)) ++failures;
    bad = tree; bad[4].bounds.left = 300;
    if (contentBounds(bad, client, true)) ++failures;
    bad = tree; bad[4].scroll = false;
    if (contentBounds(bad, client, true)) ++failures;
    bad = tree; bad[4].parent = 6;
    if (contentBounds(bad, client, true)) ++failures;
    bad = tree; bad[6].visible = false;
    if (contentBounds(bad, client, true)) ++failures;
    bad = tree; bad[3].bounds.bottom = 260;
    if (contentBounds(bad, client, true)) ++failures;
    // Negative monitor origins and non-100% physical geometry need no scaling guesses.
    for (auto& node : tree) { node.bounds.left -= 1800; node.bounds.right -= 1800; }
    if (!contentBounds(tree, {-1700, 100, -700, 900}, true)) ++failures;
    std::cout << "Settings page policy failures: " << failures << '\n';
    return failures ? 1 : 0;
}
