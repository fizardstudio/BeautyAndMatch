import React, { useState, useEffect } from 'react';
import { View, Text, TouchableOpacity, StyleSheet, BackHandler } from 'react-native';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import HomeScreen from './src/screens/HomeScreen';
import TryOnScreen from './src/screens/TryOnScreen';
import { THEME } from './src/theme';

type Tab = 'home' | 'tryon' | 'looks' | 'profile';

const ComingSoonScreen = ({ title }: { title: string }) => (
  <View style={styles.comingSoon}>
    <Text style={styles.comingSoonTitle}>{title}</Text>
    <Text style={styles.comingSoonBadge}>Segera Hadir</Text>
  </View>
);

const TabIcon = ({ active, glyph, label }: { active: boolean; glyph: string; label: string }) => (
  <View style={styles.tabItem}>
    <Text style={[styles.tabGlyph, active && styles.tabGlyphActive]}>{glyph}</Text>
    <Text style={[styles.tabLabel, active && styles.tabLabelActive]}>{label}</Text>
  </View>
);

const App = () => {
  const [activeTab, setActiveTab] = useState<Tab>('home');

  useEffect(() => {
    if (activeTab === 'home' || activeTab === 'tryon') return;
    const sub = BackHandler.addEventListener('hardwareBackPress', () => {
      setActiveTab('home');
      return true;
    });
    return () => sub.remove();
  }, [activeTab]);

  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <SafeAreaProvider>
        <View style={styles.root}>
          <View style={styles.screenArea}>
            {activeTab === 'home' && <HomeScreen />}
            {activeTab === 'tryon' && <TryOnScreen onBack={() => setActiveTab('home')} />}
            {activeTab === 'looks' && <ComingSoonScreen title="Galeri Look" />}
            {activeTab === 'profile' && <ComingSoonScreen title="Profil" />}
          </View>

          {activeTab !== 'tryon' && (
            <SafeAreaView style={styles.tabBarSafe} edges={['bottom']}>
              <View style={styles.tabBar}>
                <TouchableOpacity style={styles.tabTouch} onPress={() => setActiveTab('home')}>
                  <TabIcon active={activeTab === 'home'} glyph="⌂" label="Home" />
                </TouchableOpacity>
                <TouchableOpacity style={styles.tabTouch} onPress={() => setActiveTab('tryon')}>
                  <TabIcon active={false} glyph="◉" label="Try-On" />
                </TouchableOpacity>
                <TouchableOpacity style={styles.tabTouch} onPress={() => setActiveTab('looks')}>
                  <TabIcon active={activeTab === 'looks'} glyph="▦" label="Looks" />
                </TouchableOpacity>
                <TouchableOpacity style={styles.tabTouch} onPress={() => setActiveTab('profile')}>
                  <TabIcon active={activeTab === 'profile'} glyph="◍" label="Profil" />
                </TouchableOpacity>
              </View>
            </SafeAreaView>
          )}
        </View>
      </SafeAreaProvider>
    </GestureHandlerRootView>
  );
};

const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: THEME.voidPlum },
  screenArea: { flex: 1 },

  tabBarSafe: { backgroundColor: THEME.voidPlum },
  tabBar: {
    flexDirection: 'row',
    borderTopWidth: 1,
    borderTopColor: 'rgba(255,255,255,0.08)',
    paddingTop: 10,
    paddingBottom: 6,
  },
  tabTouch: { flex: 1, alignItems: 'center' },
  tabItem: { alignItems: 'center', gap: 4 },
  tabGlyph: { fontSize: 18, color: THEME.taupeMist },
  tabGlyphActive: { color: THEME.roseQuartz },
  tabLabel: { fontSize: 9.5, letterSpacing: 0.3, color: THEME.taupeMist },
  tabLabelActive: { color: THEME.roseQuartz, fontWeight: '600' },

  comingSoon: {
    flex: 1,
    backgroundColor: THEME.voidPlum,
    justifyContent: 'center',
    alignItems: 'center',
    gap: 10,
  },
  comingSoonTitle: {
    fontFamily: 'serif',
    fontStyle: 'italic',
    fontSize: 24,
    color: THEME.champagne,
  },
  comingSoonBadge: {
    fontSize: 11,
    letterSpacing: 1,
    textTransform: 'uppercase',
    color: THEME.glowGold,
    borderWidth: 1,
    borderColor: 'rgba(232,194,122,0.4)',
    paddingHorizontal: 12,
    paddingVertical: 5,
    borderRadius: 999,
  },
});

export default App;
