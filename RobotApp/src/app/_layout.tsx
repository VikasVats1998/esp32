import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { DarkTheme, DefaultTheme, ThemeProvider } from 'expo-router';
import { useColorScheme } from 'react-native';

import AppTabs from '@/components/app-tabs';
import { useEventPump } from '@/hooks/use-robot';

const queryClient = new QueryClient();

function Pumps() {
  useEventPump();
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
