import { ChevronLeft } from "lucide-react";
import { useLocation, useNavigate } from "react-router-dom";

import { ROUTES } from "@/shared/config";
import { cn } from "@/shared/lib";

interface BackButtonProps {
  fallback?: string;
  className?: string;
  "aria-label"?: string;
}

export function BackButton({
  fallback = ROUTES.home,
  className,
  "aria-label": ariaLabel = "Go back",
}: BackButtonProps) {
  const navigate = useNavigate();
  const location = useLocation();

  const handleClick = () => {
    if (location.key === "default") {
      void navigate(fallback, { viewTransition: true });
      return;
    }
    navigate(-1);
  };

  return (
    <button
      type="button"
      onClick={handleClick}
      className={cn(
        "flex size-9 items-center justify-center rounded-md hover:bg-muted",
        className,
      )}
      aria-label={ariaLabel}
    >
      <ChevronLeft className="size-5" aria-hidden="true" />
    </button>
  );
}
