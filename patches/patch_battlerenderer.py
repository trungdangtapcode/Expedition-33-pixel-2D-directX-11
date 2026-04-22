import re

with open('src/Battle/BattleRenderer.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

# Replace Renderer initialization
init_player_old = r'''        // Upload PNG to GPU and create SpriteBatch \+ D3D states\.
        if \(!mPlayerRenderers\[i\]\.Initialize\(device, context, info\.texturePath, sheet\)\)
        \{
            LOG\("\[BattleRenderer\] Failed to init player renderer slot %d", i\);
            return false;
        \}

        // Start the idle \(or configured start\) clip immediately so the first
        // Render\(\) call has a valid mActiveClip and does not assert\.
        mPlayerRenderers\[i\]\.PlayClip\(info\.startClip\);

        // Apply the per-slot draw offset \(e\.g\. to correct bottom-center pivot\)\.
        // Zero by default  set explicitly in SlotInfo when alignment needs tuning\.
        mPlayerRenderers\[i\]\.SetDrawOffset\(info\.drawOffsetX, info\.drawOffsetY\);
        mPlayerActive\[i\] = true;'''

init_player_new = r'''        mPlayerSprites[i] = mScene.Spawn<BattleCombatantSprite>(
            device, context, info.texturePath, sheet, 
            &mCameraCtrl.GetCamera(), 
            mPlayerWorldX[i], mPlayerWorldY[i], 2.0f, false
        );
        mPlayerSprites[i]->PlayClip(info.startClip);
        mPlayerSprites[i]->SetDrawOffset(info.drawOffsetX, info.drawOffsetY);
        mPlayerActive[i] = true;'''

text = re.sub(init_player_old, init_player_new, text)

# Equivalent for Enemy slots (approximate)
init_enemy_old = r'''        // Upload PNG to GPU and create SpriteBatch \+ D3D states\.
        if \(!mEnemyRenderers\[i\]\.Initialize\(device, context, info\.texturePath, sheet\)\)
        \{
            LOG\("\[BattleRenderer\] Failed to init enemy renderer slot %d", i\);
            return false;
        \}

        // Start the idle \(or configured start\) clip immediately so the first
        // Render\(\) call has a valid mActiveClip and does not assert\.
        mEnemyRenderers\[i\]\.PlayClip\(info\.startClip\);

        // Apply the per-slot draw offset \(e\.g\. to correct bottom-center pivot\)\.
        // Zero by default  set explicitly in SlotInfo when alignment needs tuning\.
        mEnemyRenderers\[i\]\.SetDrawOffset\(info\.drawOffsetX, info\.drawOffsetY\);
        mEnemyActive\[i\] = true;'''
        
init_enemy_new = r'''        mEnemySprites[i] = mScene.Spawn<BattleCombatantSprite>(
            device, context, info.texturePath, sheet, 
            &mCameraCtrl.GetCamera(), 
            mEnemyWorldX[i], mEnemyWorldY[i], 2.0f, true
        );
        mEnemySprites[i]->PlayClip(info.startClip);
        mEnemySprites[i]->SetDrawOffset(info.drawOffsetX, info.drawOffsetY);
        mEnemyActive[i] = true;'''

text = re.sub(init_enemy_old, init_enemy_new, text)

update_old = r'''void BattleRenderer::Update\(float dt\)
\{
    mCameraCtrl\.Update\(dt\);

    for \(int i = 0; i < kMaxSlots; \+\+i\)
    \{
        if \(mPlayerActive\[i\]\) mPlayerRenderers\[i\]\.Update\(dt\);
        if \(mEnemyActive\[i\]\)  mEnemyRenderers\[i\]\.Update\(dt\);
    \}
\}'''

update_new = r'''void BattleRenderer::Update(float dt)
{
    mCameraCtrl.Update(dt);
    mScene.Update(dt);
}'''
text = re.sub(update_old, update_new, text)

render_old = r'''void BattleRenderer::Render\(ID3D11DeviceContext\* context\)
\{
    // Retrieve the interpolated camera from the controller\.
    // The matrix was already rebuilt in Update\(\)  no redundant rebuild here\.
    Camera2D& cam = mCameraCtrl\.GetCamera\(\);

    // -- Player side ------------------------------------------------
    // Players face right \(default sprite orientation\)  flipX=false\.
    for \(int i = 0; i < kMaxSlots; \+\+i\)
    \{
        if \(!mPlayerActive\[i\]\) continue;
        mPlayerRenderers\[i\]\.Draw\(
            context,
            cam,
            mPlayerWorldX\[i\],   // world x  no conversion needed
            mPlayerWorldY\[i\],   // world y  no conversion needed
            2\.0f,               // scale up for visibility
            false               // face right
        \);
    \}

    // -- Enemy side -------------------------------------------------
    // Enemies face left \(flipX=true\) so they face the player side\.
    for \(int i = 0; i < kMaxSlots; \+\+i\)
    \{
        if \(!mEnemyActive\[i\]\) continue;
        mEnemyRenderers\[i\]\.Draw\(
            context,
            cam,
            mEnemyWorldX\[i\],    // world x  no conversion needed
            mEnemyWorldY\[i\],    // world y  no conversion needed
            2\.0f,               // scale up for visibility
            true                // face left  mirror the sprite
        \);
    \}
\}'''
render_new = r'''void BattleRenderer::Render(ID3D11DeviceContext* context)
{
    mScene.Render(context);
}'''

text = re.sub(render_old, render_new, text)

shutdown_old = r'''void BattleRenderer::Shutdown\(\)
\{
    for \(int i = 0; i < kMaxSlots; \+\+i\)
    \{
        mPlayerRenderers\[i\]\.Shutdown\(\);
        mEnemyRenderers\[i\]\.Shutdown\(\);
    \}
\}'''

shutdown_new = r'''void BattleRenderer::Shutdown()
{
    mScene.Clear();
}'''
text = re.sub(shutdown_old, shutdown_new, text)

# Replace all PlayClip, IsClipDone, FreezeCurrentFrame for Player/EnemyRenderers back to Sprites
text = text.replace('mEnemyRenderers[slot].PlayClip', 'mEnemySprites[slot]->PlayClip')
text = text.replace('mPlayerRenderers[slot].PlayClip', 'mPlayerSprites[slot]->PlayClip')

text = text.replace('mEnemyRenderers[slot].FreezeCurrentFrame()', 'mEnemySprites[slot]->FreezeCurrentFrame()')
text = text.replace('mPlayerRenderers[slot].FreezeCurrentFrame()', 'mPlayerSprites[slot]->FreezeCurrentFrame()')

text = text.replace('mEnemyRenderers[i].IsClipDone()', 'mEnemySprites[i]->IsClipDone()')
text = text.replace('mPlayerRenderers[i].IsClipDone()', 'mPlayerSprites[i]->IsClipDone()')
text = text.replace('!mEnemyRenderers[slot].IsClipDone()', '!mEnemySprites[slot]->IsClipDone()')

with open('src/Battle/BattleRenderer.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
