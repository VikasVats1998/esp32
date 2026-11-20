import { NativeTabs } from 'expo-router/native-tabs';
import { useColorScheme } from 'react-native';

import { Colors } from '@/constants/theme';

export default function AppTabs() {
  const scheme = useColorScheme();
  const colors = Colors[scheme === 'dark' ? 'dark' : 'light'];

  return (
    <NativeTabs
      backgroundColor={colors.background}
      indicatorColor={colors.backgroundElement}
      labelStyle={{ selected: { color: colors.text } }}>
      <NativeTabs.Trigger name="index">
        <NativeTabs.Trigger.Label>Robots</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="antenna.radiowaves.left.and.right" md="sensors" />
      </NativeTabs.Trigger>
      <NativeTabs.Trigger name="control">
        <NativeTabs.Trigger.Label>Control</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="gamecontroller.fill" md="sports_esports" />
      </NativeTabs.Trigger>
      <NativeTabs.Trigger name="test">
        <NativeTabs.Trigger.Label>Test</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="checklist" md="fact_check" />
      </NativeTabs.Trigger>
      <NativeTabs.Trigger name="servos">
        <NativeTabs.Trigger.Label>Servos</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="slider.horizontal.3" md="tune" />
      </NativeTabs.Trigger>
      <NativeTabs.Trigger name="settings">
        <NativeTabs.Trigger.Label>Settings</NativeTabs.Trigger.Label>
        <NativeTabs.Trigger.Icon sf="gearshape.fill" md="settings" />
      </NativeTabs.Trigger>
    </NativeTabs>
  );
}
