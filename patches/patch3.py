
import codecs
import re

def patch():
    # PATCH HEADER
    with codecs.open("src/UI/TurnQueueUI.h", "r", "utf-8") as f:
        h_content = f.read()
        
    new_struct = """struct QueueNode {
        IBattler* battler = nullptr;
        std::wstring portraitPath;
        float currentY = 0.0f;
        float targetY = 0.0f;
        float currentX = 0.0f;
        float targetX = 0.0f;
        float currentScale = 0.0f;
        float targetScale = 0.0f;
        float alpha = 1.0f;
        bool matched = false;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };"""
    
    h_content = re.sub(r"struct QueueNode\s*\{.*?\};", new_struct, h_content, flags=re.DOTALL)
    if "std::vector<QueueNode> mFadingNodes;" not in h_content:
        h_content = h_content.replace("std::vector<QueueNode> mNodes;", "std::vector<QueueNode> mNodes;\n    std::vector<QueueNode> mFadingNodes;")
        
    with codecs.open("src/UI/TurnQueueUI.h", "w", "utf-8") as f:
        f.write(h_content)
        
    # PATCH CPP
    with codecs.open("src/UI/TurnQueueUI.cpp", "r", "utf-8") as f:
        c_content = f.read()
        
    if "mFadingNodes.clear();" not in c_content:
        c_content = c_content.replace("mNodes.clear();", "mNodes.clear();\n    mFadingNodes.clear();")
        
    if "mFadingNodes.push_back(oldNode);" not in c_content:
        c_content = c_content.replace("mNodes = std::move(newNodes);", """for (auto& oldNode : mNodes) {
        if (!oldNode.matched) {
            oldNode.targetX -= 150.0f; // Pop off to the left a bit
            oldNode.targetScale *= 1.2f; // Slight pop effect
            mFadingNodes.push_back(oldNode);
        }
    }
    mNodes = std::move(newNodes);""")

    if "// Update Fading Nodes" not in c_content:
        update_find = """    for (auto& node : mNodes) {
        node.currentX += (node.targetX - node.currentX) * mConfig.animSpeed * dt;
        node.currentY += (node.targetY - node.currentY) * mConfig.animSpeed * dt;
        node.currentScale += (node.targetScale - node.currentScale) * mConfig.animSpeed * dt;
    }"""
        update_replacement = """    for (auto& node : mNodes) {
        node.currentX += (node.targetX - node.currentX) * mConfig.animSpeed * dt;
        node.currentY += (node.targetY - node.currentY) * mConfig.animSpeed * dt;
        node.currentScale += (node.targetScale - node.currentScale) * mConfig.animSpeed * dt;
    }

    // Update Fading Nodes
    for (auto it = mFadingNodes.begin(); it != mFadingNodes.end(); ) {
        it->currentX += (it->targetX - it->currentX) * mConfig.animSpeed * dt;
        it->currentY += (it->targetY - it->currentY) * mConfig.animSpeed * dt;
        it->currentScale += (it->targetScale - it->currentScale) * mConfig.animSpeed * dt;
        it->alpha -= 3.0f * dt; // Fade out over ~0.33 seconds
        
        if (it->alpha <= 0.0f) {
            it = mFadingNodes.erase(it);
        } else {
            ++it;
        }
    }"""
        c_content = c_content.replace(update_find, update_replacement)
        
    if "XMVectorSet" not in c_content:
        render_find = """        XMVECTOR color = Colors::White;

        if (mBgSRV) {"""
        render_replacement = """        XMVECTOR color = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, node.alpha);

        if (mBgSRV) {"""
        c_content = c_content.replace(render_find, render_replacement)
        
        end_find = "    mSpriteBatch->End();"
        fading_render = """    // Render fading out nodes on top
    for (const auto& node : mFadingNodes)
    {
        RECT destRect;
        destRect.left   = static_cast<LONG>(node.currentX);
        destRect.top    = static_cast<LONG>(node.currentY);
        destRect.right  = static_cast<LONG>(node.currentX + mConfig.width * node.currentScale);
        destRect.bottom = static_cast<LONG>(node.currentY + mConfig.height * node.currentScale);

        XMVECTOR color = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, node.alpha);

        if (mBgSRV)   mSpriteBatch->Draw(mBgSRV.Get(), destRect, color);
        if (node.srv) mSpriteBatch->Draw(node.srv.Get(), destRect, color);
        if (mFrameSRV) mSpriteBatch->Draw(mFrameSRV.Get(), destRect, color);
    }
    
    mSpriteBatch->End();"""
        c_content = c_content.replace(end_find, fading_render)

    with codecs.open("src/UI/TurnQueueUI.cpp", "w", "utf-8") as f:
        f.write(c_content)

patch()

