// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// The game page's Online Saves section: the Google Play Games player a game's own online saves are kept under.
///
/// A game that saves to its own server under its Play Games player (Idling to Rule the Gods' Save Online / Load Online) gets
/// the same player as on Android when Husk's Store is signed in with the person's Google account (Play/PlayGamesSignIn.swift):
/// the game then loads and saves its online progress itself. This shows which account and player that is, checks the sign-in
/// on demand, and copies what is needed to look into a sign-in that does not work.
struct OnlineSavesSection: View {
    let app: TLApp

    @ObservedObject private var store = PlayStoreManager.shared
    @State private var showSignIn = false
    @State private var checking = false
    @State private var player: PlayGamesSignIn.Player?
    @State private var message: String?

    private var googleAccount: PlaySession? {
        guard let s = store.session, !s.isAnonymous, !s.aasToken.isEmpty else { return nil }
        return s
    }

    var body: some View {
        Section {
            if let s = googleAccount {
                LabeledContent("Google Account", value: s.email)
                if let p = player ?? PlayGamesSignIn.lastPlayer(package: app.packageName) {
                    LabeledContent("Play Games Player", value: p.name.isEmpty ? "—" : p.name)
                    LabeledContent("Player ID", value: p.id).textSelection(.enabled)
                }
                Button { check() } label: {
                    Label(checking ? "Signing In to Play Games…" : "Check Play Games Sign-In", systemImage: "person.crop.circle.badge.checkmark")
                }
                .disabled(checking)
            } else {
                Button { showSignIn = true } label: {
                    Label(store.session?.isAnonymous == true ? "Sign In with Your Own Google Account" : "Sign In with Google",
                          systemImage: "person.badge.key")
                }
            }
            Button {
                UIPasteboard.general.string = PlayGamesSignIn.details(app)
                message = "Sign-in details copied. They name the account, the game, its certificate and the last error; no passwords or tokens."
            } label: {
                Label("Copy Sign-In Details", systemImage: "doc.on.doc")
            }
        } header: {
            Text("Online Saves")
        } footer: {
            VStack(alignment: .leading, spacing: 6) {
                if let message { Text(message) }
                else if googleAccount != nil, let why = PlayGamesSignIn.lastFailure(package: app.packageName), player == nil {
                    Text("Last sign-in failed: \(why)")
                }
                if googleAccount == nil {
                    Text("Games that save online with Google Play Games (Idling to Rule the Gods' Save Online and Load Online) need your own Google account. Sign in with the account you play with on Android, and the game finds the same player and the same online saves.")
                } else {
                    Text("\(app.label) signs in to Google Play Games as this account's player, as it does on Android. Its online saves and loads go to the game's own servers.")
                }
            }
        }
        .sheet(isPresented: $showSignIn) { GoogleSignInSheet() }
    }

    private func check() {
        checking = true; message = nil
        let game = PlayGamesSignIn.Game(package: app.packageName, apk: app.apks.first)
        let label = app.label
        DispatchQueue.global(qos: .userInitiated).async {
            let r = PlayGamesSignIn.player(game, interactive: true)
            DispatchQueue.main.async {
                checking = false
                switch r {
                case .success(let p): player = p; message = "Signed in to Play Games for \(label)."
                case .failure(let e): player = nil; message = e.errorDescription
                }
            }
        }
    }
}
