import React, { useMemo } from 'react';
import {
  View,
  Text,
  ScrollView,
  StyleSheet,
  TouchableOpacity,
  Platform,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { THEME } from '../theme';

const HomeScreen = () => {
  const formattedDate = useMemo(() => {
    const today = new Date();
    const dayNames = [
      'Minggu',
      'Senin',
      'Selasa',
      'Rabu',
      'Kamis',
      'Jumat',
      'Sabtu',
    ];
    const monthNames = [
      'Januari',
      'Februari',
      'Maret',
      'April',
      'Mei',
      'Juni',
      'Juli',
      'Agustus',
      'September',
      'Oktober',
      'November',
      'Desember',
    ];
    const dayName = dayNames[today.getDay()];
    const date = today.getDate();
    const monthName = monthNames[today.getMonth()];
    return `${dayName}, ${date} ${monthName}`;
  }, []);

  return (
    <SafeAreaView style={styles.safeArea}>
      {/* Background glow accents */}
      <View style={[styles.glowAccent, styles.glowTopLeft]} />
      <View style={[styles.glowAccent, styles.glowTopRight]} />

      <ScrollView
        contentContainerStyle={styles.scrollContent}
        showsVerticalScrollIndicator={false}
      >
        {/* Kicker text */}
        <Text style={styles.kicker}>SELAMAT DATANG</Text>

        {/* Greeting title */}
        <Text style={styles.greetingTitle}>Siap tampil{'\n'}hari ini?</Text>

        {/* Date subtitle */}
        <Text style={styles.dateSubtitle}>{formattedDate}</Text>

        {/* Hero card */}
        <View style={styles.heroCard}>
          {/* Gradient blob effect */}
          <View style={styles.gradientBlob} />

          <Text style={styles.heroEyebrow}>LIVE AR</Text>
          <Text style={styles.heroTitle}>Try-On sekarang</Text>
          <TouchableOpacity style={styles.heroCta} activeOpacity={0.7}>
            <Text style={styles.heroCtaText}>Mulai</Text>
          </TouchableOpacity>
        </View>

        {/* Features section header */}
        <View style={styles.sectionHeader}>
          <Text style={styles.sectionTitle}>Fitur lainnya</Text>
          <Text style={styles.sectionCount}>6 total</Text>
        </View>

        {/* Feature grid */}
        <View style={styles.featureGrid}>
          <FeatureCard
            title={'AI Skin\nDiagnostics'}
            icon="◆"
            badge="Segera"
          />
          <FeatureCard
            title={'Shade\nMatcher'}
            icon="●"
            badge="Segera"
          />
          <FeatureCard
            title={'Galeri\nLook'}
            icon="★"
            badge="Segera"
          />
          <FeatureCard title={'Riwayat\nScan'} icon="◎" />
        </View>

        {/* Bottom padding */}
        <View style={styles.bottomPadding} />
      </ScrollView>
    </SafeAreaView>
  );
};

interface FeatureCardProps {
  title: string;
  icon: string;
  badge?: string;
}

const FeatureCard: React.FC<FeatureCardProps> = ({ title, icon, badge }) => {
  return (
    <View style={styles.featureCard}>
      {badge && (
        <View style={styles.badgeContainer}>
          <Text style={styles.badgeText}>{badge}</Text>
        </View>
      )}

      <View style={styles.iconBadge}>
        <Text style={styles.iconText}>{icon}</Text>
      </View>

      <Text style={styles.featureTitle}>{title}</Text>
    </View>
  );
};

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: THEME.voidPlum,
  },

  scrollContent: {
    paddingHorizontal: 20,
    paddingTop: 16,
  },

  // Background glow accents
  glowAccent: {
    position: 'absolute',
    borderRadius: 999,
    opacity: 0.15,
  },
  glowTopLeft: {
    width: 280,
    height: 280,
    top: -80,
    left: -60,
    backgroundColor: THEME.irisShimmer,
  },
  glowTopRight: {
    width: 320,
    height: 320,
    top: -100,
    right: -80,
    backgroundColor: THEME.roseQuartz,
  },

  // Kicker
  kicker: {
    fontSize: 11,
    fontWeight: '700',
    letterSpacing: 1.5,
    color: THEME.irisShimmer,
    marginBottom: 8,
  },

  // Greeting title
  greetingTitle: {
    fontSize: 26,
    fontWeight: '500',
    fontStyle: 'italic',
    fontFamily: Platform.OS === 'ios' ? 'Georgia' : 'serif',
    color: THEME.champagne,
    marginBottom: 8,
    lineHeight: 32,
  },

  // Date subtitle
  dateSubtitle: {
    fontSize: 12,
    color: THEME.taupeMist,
    marginBottom: 28,
  },

  // Hero card
  heroCard: {
    height: 150,
    borderRadius: 24,
    backgroundColor: THEME.roseQuartz,
    padding: 16,
    marginBottom: 32,
    justifyContent: 'space-between',
    overflow: 'hidden',
  },

  // Gradient blob (fake gradient effect)
  gradientBlob: {
    position: 'absolute',
    width: 120,
    height: 120,
    borderRadius: 999,
    backgroundColor: THEME.irisShimmer,
    opacity: 0.5,
    bottom: -30,
    right: -40,
  },

  heroEyebrow: {
    fontSize: 10,
    fontWeight: '600',
    color: 'rgba(30, 12, 20, 0.65)',
    marginBottom: 4,
  },

  heroTitle: {
    fontSize: 23,
    fontWeight: '500',
    fontStyle: 'italic',
    fontFamily: Platform.OS === 'ios' ? 'Georgia' : 'serif',
    color: '#241220',
  },

  heroCta: {
    alignSelf: 'flex-start',
    backgroundColor: '#201118',
    paddingHorizontal: 18,
    paddingVertical: 8,
    borderRadius: 999,
  },

  heroCtaText: {
    fontSize: 11,
    fontWeight: '600',
    color: THEME.champagne,
  },

  // Section header
  sectionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },

  sectionTitle: {
    fontSize: 13,
    fontWeight: '700',
    color: THEME.champagne,
  },

  sectionCount: {
    fontSize: 11,
    color: THEME.taupeMist,
  },

  // Feature grid
  featureGrid: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    justifyContent: 'space-between',
    marginBottom: 12,
  },

  featureCard: {
    width: '48%',
    minHeight: 96,
    borderRadius: 18,
    backgroundColor: 'rgba(255, 255, 255, 0.045)',
    borderWidth: 1,
    borderColor: 'rgba(255, 255, 255, 0.08)',
    padding: 14,
    marginBottom: 14,
    justifyContent: 'flex-start',
  },

  iconBadge: {
    width: 30,
    height: 30,
    borderRadius: 15,
    backgroundColor: 'rgba(242, 167, 173, 0.16)',
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 10,
  },

  iconText: {
    fontSize: 14,
    color: THEME.roseQuartz,
    fontWeight: '600',
  },

  featureTitle: {
    fontSize: 12.5,
    fontWeight: '600',
    color: THEME.champagne,
    lineHeight: 16,
  },

  // Badge
  badgeContainer: {
    position: 'absolute',
    top: 8,
    right: 8,
    borderWidth: 1,
    borderColor: THEME.glowGold,
    borderRadius: 999,
    paddingHorizontal: 8,
    paddingVertical: 4,
  },

  badgeText: {
    fontSize: 8.5,
    fontWeight: '700',
    color: THEME.glowGold,
    letterSpacing: 0.5,
    textTransform: 'uppercase',
  },

  // Bottom padding
  bottomPadding: {
    height: 24,
  },
});

export default HomeScreen;
