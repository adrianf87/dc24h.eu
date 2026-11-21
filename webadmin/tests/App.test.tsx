import { cleanup, render, screen, within } from '@testing-library/react'
import { afterEach, expect, test } from 'vitest'
import App from '../src/App'

afterEach(cleanup)

test('shows the project and release on the Webadmin welcome screen', () => {
  render(<App />)

  const main = within(screen.getByRole('main'))
  expect(main.getByRole('heading', { level: 1 }).textContent).toBe(
    'dc24h.eu — ADC/ADCS Hub — Alpha 0.1',
  )
})
