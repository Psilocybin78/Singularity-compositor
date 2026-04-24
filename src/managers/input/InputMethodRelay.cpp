#include "InputMethodRelay.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../event/EventBus.hpp"
#include "../../protocols/TextInputV3.hpp"
#include "../../protocols/TextInputV1.hpp"
#include "../../protocols/InputMethodV2.hpp"
#include "../../protocols/core/Compositor.hpp"

CInputMethodRelay::CInputMethodRelay() {
    static auto P = Event::bus()->m_events.input.keyboard.focus.listen([&](SP<CWLSurfaceResource> surf) { onKeyboardFocus(surf); });

    m_listeners.newTIV3 = PROTO::textInputV3->m_events.newTextInput.listen([this](const auto& input) { onNewTextInput(input); });
    m_listeners.newTIV1 = PROTO::textInputV1->m_events.newTextInput.listen([this](const auto& input) { onNewTextInput(input); });
    m_listeners.newIME  = PROTO::ime->m_events.newIME.listen([this](const auto& ime) { onNewIME(ime); });
}

void CInputMethodRelay::onNewIME(SP<CInputMethodV2> pIME) {
    if (!m_inputMethod.expired()) {
        Log::logger->log(Log::ERR, "Cannot register 2 IMEs at once!");

        pIME->unavailable();

        return;
    }

    m_inputMethod = pIME;

    m_listeners.commitIME = pIME->m_events.onCommit.listen([this] {
        const auto PTI = getFocusedTextInput();

        if (!PTI) {
            Log::logger->log(Log::DEBUG, "No focused TextInput on IME Commit");
            return;
        }

        PTI->updateIMEState(m_inputMethod.lock());
    });

    m_listeners.destroyIME = pIME->m_events.destroy.listen([this] {
        const auto PTI = getFocusedTextInput();

        Log::logger->log(Log::DEBUG, "IME Destroy");

        if (PTI)
            PTI->leave();

        m_inputMethod.reset();
    });

    m_listeners.newPopup = pIME->m_events.newPopup.listen([this](const SP<CInputMethodPopupV2>& popup) {
        m_inputMethodPopups.emplace_back(makeUnique<CInputPopup>(popup));
        Log::logger->log(Log::DEBUG, "New input popup");
    });

    if (!Desktop::focusState()->surface())
        return;

    for (auto const& ti : m_textInputs) {
        if (ti->client() != Desktop::focusState()->surface()->client())
            continue;

        if (ti->isV3())
            ti->enter(Desktop::focusState()->surface());
        else
            ti->onEnabled(Desktop::focusState()->surface());
    }
}

void CInputMethodRelay::removePopup(CInputPopup* pPopup) {
    std::erase_if(m_inputMethodPopups, [pPopup](const auto& other) { return other.get() == pPopup; });
}

CTextInput* CInputMethodRelay::getFocusedTextInput() {
    if (!Desktop::focusState()->surface())
        return nullptr;

    for (auto const& ti : m_textInputs) {
        if (ti->focusedSurface() == Desktop::focusState()->surface() && ti->isEnabled())
            return ti.get();
    }

    for (auto const& ti : m_textInputs) {
        if (ti->focusedSurface() == Desktop::focusState()->surface())
            return ti.get();
    }

    return nullptr;
}

void CInputMethodRelay::onNewTextInput(WP<CTextInputV3> tiv3) {
    m_textInputs.emplace_back(makeUnique<CTextInput>(tiv3));
}

void CInputMethodRelay::onNewTextInput(WP<CTextInputV1> pTIV1) {
    m_textInputs.emplace_back(makeUnique<CTextInput>(pTIV1));
}

void CInputMethodRelay::removeTextInput(CTextInput* pInput) {
    std::erase_if(m_textInputs, [pInput](const auto& other) { return other.get() == pInput; });
}

void CInputMethodRelay::updateAllPopups() {
    for (auto const& p : m_inputMethodPopups) {
        p->onCommit();
    }
}

void CInputMethodRelay::activateIME(CTextInput* pInput, bool shouldCommit) {
    if (m_inputMethod.expired())
        return;

    m_inputMethod->activate();
    if (shouldCommit)
        commitIMEState(pInput);
}

void CInputMethodRelay::deactivateIME(CTextInput* pInput, bool shouldCommit) {
    if (m_inputMethod.expired())
        return;

    m_inputMethod->deactivate();
    if (shouldCommit)
        commitIMEState(pInput);
}

void CInputMethodRelay::commitIMEState(CTextInput* pInput) {
    if (m_inputMethod.expired())
        return;

    pInput->commitStateToIME(m_inputMethod.lock());
}

void CInputMethodRelay::onKeyboardFocus(SP<CWLSurfaceResource> pSurface) {
    // Singularity patch: do NOT gate text-input-v3 enter/leave on
    // having an IME registered. Upstream's `if (m_inputMethod.expired())
    // return;` guard at the top of this function silently drops
    // zwp_text_input_v3.enter delivery on every focus change when no
    // IME (fcitx, ibus, etc.) is connected to the seat — but
    // text-input-v3 is *also* the protocol that browsers and Electron
    // apps with `--enable-wayland-ime --wayland-text-input-version=3`
    // use to receive input in their built-in Wayland IME path. Without
    // the enter event, those clients latch into "I'm not focused for
    // text" state after the first focus change and typing dies until
    // a real-mouse-click resyncs them via processMouseDownNormal →
    // refocus.
    //
    // The wire-level enter/leave calls below have no IME dependency —
    // they're plain `zwp_text_input_v3.enter(surface)` / `.leave()`
    // protocol events. CTextInput::leave() does call deactivateIME()
    // at the end, but deactivateIME has its own m_inputMethod-expired
    // gate, so the no-IME case is already handled correctly.
    //
    // Bug verified live via uprobe on a clean Singularity install
    // running Claude Desktop (Electron, --wayland-text-input-version=3,
    // no IME): hypr_imr_kbd_focus fires, hypr_kbd_enter_entry fires,
    // hypr_ti_enter never fires. After this patch, ti_enter fires too.
    if (pSurface == m_lastKbFocus)
        return;

    m_lastKbFocus = pSurface;

    for (auto const& ti : m_textInputs) {
        if (!ti->focusedSurface())
            continue;

        ti->leave();
    }

    if (!pSurface)
        return;

    for (auto const& ti : m_textInputs) {
        if (!ti->isV3())
            continue;

        if (ti->client() != pSurface->client())
            continue;

        ti->enter(pSurface);
    }
}

CInputPopup* CInputMethodRelay::popupFromCoords(const Vector2D& point) {
    for (auto const& p : m_inputMethodPopups) {
        if (p->isVecInPopup(point))
            return p.get();
    }

    return nullptr;
}

CInputPopup* CInputMethodRelay::popupFromSurface(const SP<CWLSurfaceResource> surface) {
    for (auto const& p : m_inputMethodPopups) {
        if (p->getSurface() == surface)
            return p.get();
    }

    return nullptr;
}
