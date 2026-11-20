import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { DarkTheme, DefaultTheme, ThemeProvider } from 'expo-router';
import { useColorScheme } from 'react-native';

import AppTabs from '@/components/app-tabs';
import { useConnectionNotes, useEventPump, useLogPump } from '@/hooks/use-robot';
import { setRandomProvider } from '@/desk/crypto';
import { installFeedback } from '@/lib/feedback';
import { useDesk } from '@/store/desk';
import { getRandomBytes } from 'expo-crypto';
import { useEffect } from 'react';

const queryClient = new QueryClient();
installFeedback();
setRandomProvider((n) => getRandomBytes(n));

function Pumps() {
  const load = useDesk((s) => s.load);
  useEffect(() => {
    load();
  }, [load]);
  useEventPump();
  useLogPump();
  useConnectionNotes();
  return null;
}

export default function RootLayout() {
  const colorScheme = useColorScheme();
  return (
    <QueryClientProvider client={queryClient}>
      <ThemeProvider value={colorScheme === 'dark' ? DarkTheme : DefaultTheme}>
        <Pumps />
        <AppTabs />
      </ThemeProvider>
    </QueryClientProvider>
  );
}
